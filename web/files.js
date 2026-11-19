//! The browser file chooser behind FileDialog: picked files mount read-only under /mnt (BrowserMountWeb.cpp), and an app's WEB_PRE_JS may replace Module.rmlkitChoose.

const rmlkitReaderSource = `
onmessage = event => {
    const {files, memory, slot} = event.data;
    const reader = new FileReaderSync();
    postMessage('ready');
    for (;;) {
        let control = new Int32Array(memory.buffer, slot, 6);
        for (let state = Atomics.load(control, 0); state !== 1; state = Atomics.load(control, 0))
            Atomics.wait(control, 0, state);
        control = new Int32Array(memory.buffer, slot, 6);
        const file = files[control[1]];
        const buffer = control[2] >>> 0;
        const size = control[3] >>> 0;
        const offset = new Float64Array(memory.buffer, slot + 24, 1)[0];
        try {
            const data = new Uint8Array(reader.readAsArrayBuffer(file.slice(offset, offset + size)));
            new Uint8Array(memory.buffer).set(data, buffer);
            control[4] = data.length;
        } catch (error) {
            console.error('Reading ' + file.name + ' failed:', error);
            control[4] = -1;
        }
        Atomics.store(control, 0, 2);
        Atomics.notify(control, 0);
    }
};`;

const rmlkitReaderCount = 4;
let rmlkitMountCount = 0;

function rmlkitExtensions(extension) {
    return extension ? extension.split(';').filter(Boolean).map(name => '.' + name.toLowerCase()) : null;
}

function rmlkitMatches(name, extensions) {
    const lower = name.toLowerCase();
    return !extensions || extensions.some(extension => lower.endsWith(extension));
}

//! Mounts the entries ({path relative to root, file}) read-only under root, after removing what root held before.
async function rmlkitMount(root, entries) {
    Module.ccall('rmlkit_mount_remove', null, ['string'], [root]);
    const mount = Module.ccall('rmlkit_mount_open', 'number', ['string', 'number'], [root, rmlkitReaderCount]);
    const files = entries.map(entry => entry.file);
    const url = URL.createObjectURL(new Blob([rmlkitReaderSource], {type: 'text/javascript'}));
    try {
        await Promise.all(Array.from({length: rmlkitReaderCount}, (_, reader) => new Promise((resolve, reject) => {
            const worker = new Worker(url);
            worker.onmessage = resolve;
            worker.onerror = reject;
            worker.postMessage({files, memory: wasmMemory, slot: Module._rmlkit_mount_slot(mount, reader)});
        })));
    } finally {
        URL.revokeObjectURL(url);
    }
    entries.forEach((entry, index) => {
        const result = Module.ccall('rmlkit_mount_add', 'number', ['number', 'string', 'number', 'number', 'number'],
                                    [mount, entry.path, index, entry.file.size, entry.file.lastModified]);
        if (result < 0) throw new Error(`Cannot add ${entry.path} to ${root}: error ${-result}.`);
    });
}

async function rmlkitCollect(directory) {
    const handles = [];
    async function visit(folder, prefix) {
        for await (const entry of folder.values()) {
            if (entry.kind === 'directory') await visit(entry, prefix + entry.name + '/');
            else handles.push({path: prefix + entry.name, handle: entry});
        }
    }
    await visit(directory, '');
    return Promise.all(handles.map(async entry => ({path: entry.path, file: await entry.handle.getFile()})));
}

//! A folder gives its directory handle, or its files with webkitRelativePath; files give an array of File.
function rmlkitPick(folder, extensions) {
    if (folder && window.showDirectoryPicker)
        return window.showDirectoryPicker({id: 'rmlkit-folder', mode: 'read'});
    if (!folder && window.showOpenFilePicker) {
        //! The accept-all option stays, so the files that a picked file refers to can be picked with it.
        const types = extensions ? [{description: 'Files', accept: {'application/octet-stream': extensions}}] : undefined;
        return window.showOpenFilePicker({id: 'rmlkit-files', multiple: true, types})
            .then(handles => Promise.all(handles.map(handle => handle.getFile())));
    }
    return new Promise((resolve, reject) => {
        const input = document.createElement('input');
        input.type = 'file';
        input.webkitdirectory = folder;
        input.multiple = !folder;
        input.style.display = 'none';
        input.addEventListener('cancel', () => {
            input.remove();
            reject(new DOMException('The picker was closed.', 'AbortError'));
        }, {once: true});
        input.addEventListener('change', () => {
            input.remove();
            resolve(Array.from(input.files));
        }, {once: true});
        document.body.appendChild(input);
        input.click();
    });
}

//! kind is "files" or a folder kind; extension is a ;-separated list without dots. Resolves the mounted path, or null when cancelled.
Module.rmlkitChoose ??= async (kind, extension) => {
    const folder = kind !== 'files';
    const extensions = rmlkitExtensions(extension);
    let picked;
    try {
        picked = await rmlkitPick(folder, extensions);
    } catch (error) {
        if (error.name === 'AbortError') return null;
        throw error;
    }
    const root = `/mnt/${folder ? kind : 'files'}/${rmlkitMountCount++}`;
    if (!Array.isArray(picked)) {
        await rmlkitMount(root, await rmlkitCollect(picked));
        return root;
    }
    if (!picked.length) throw new Error(folder ? 'The picked folder has no files.' : 'No file was picked.');
    if (folder) {
        const top = picked[0].webkitRelativePath.split('/')[0];
        await rmlkitMount(root, picked.map(file => ({path: file.webkitRelativePath.slice(top.length + 1), file})));
        return root;
    }
    //! Several picked files mount side by side, so the chosen one finds the others next to it. The chosen one is the first that matches the filter.
    await rmlkitMount(root, picked.map(file => ({path: file.name, file})));
    const chosen = picked.find(file => rmlkitMatches(file.name, extensions)) ?? picked[0];
    return `${root}/${chosen.name}`;
};
