addToLibrary({
    rmlkit_choose__deps: ['$UTF8ToString'],
    rmlkit_choose__sig: 'vpp',
    rmlkit_choose: (kind, extension) => {
        Module.rmlkitChoose(UTF8ToString(kind), UTF8ToString(extension))
            .then(path => path === null ? Module.ccall('rmlkit_dialog_cancelled', null, [], [])
                                        : Module.ccall('rmlkit_dialog_chosen', null, ['string'], [path]))
            .catch(error => Module.ccall('rmlkit_dialog_error', null, ['string'], [String(error)]));
    },

    rmlkit_show_link_drag__deps: ['$UTF8ToString'],
    rmlkit_show_link_drag__sig: 'vffffp',
    rmlkit_show_link_drag: (x, y, width, height, link) => {
        Module.rmlkitShowLinkDrag(x, y, width, height, UTF8ToString(link));
    },

    rmlkit_copy_text__proxy: 'sync',
    rmlkit_copy_text__deps: ['$UTF8ToString'],
    rmlkit_copy_text__sig: 'vp',
    rmlkit_copy_text: (text) => {
        Module.rmlkitCopy(UTF8ToString(text));
    },

    rmlkit_hide_link_drag__sig: 'v',
    rmlkit_hide_link_drag: () => {
        Module.rmlkitHideLinkDrag();
    },

    rmlkit_read_url_view__deps: ['$stringToNewUTF8'],
    rmlkit_read_url_view__sig: 'p',
    rmlkit_read_url_view: () => stringToNewUTF8(decodeURIComponent(location.pathname.slice(1))),

    rmlkit_push_url_view__deps: ['$UTF8ToString'],
    rmlkit_push_url_view__sig: 'vp',
    rmlkit_push_url_view: (view) => {
        const path = '/' + encodeURIComponent(UTF8ToString(view));
        if (location.pathname !== path)
            history.pushState(null, '', path);
    },

    rmlkit_listen_url_view__sig: 'v',
    rmlkit_listen_url_view: () => {
        Module.rmlkitUrlListener = () => {
            Module.ccall('rmlkit_url_view', null, ['string'], [decodeURIComponent(location.pathname.slice(1))]);
        };
        window.addEventListener('popstate', Module.rmlkitUrlListener);
    },

    rmlkit_unlisten_url_view__sig: 'v',
    rmlkit_unlisten_url_view: () => {
        window.removeEventListener('popstate', Module.rmlkitUrlListener);
        delete Module.rmlkitUrlListener;
    },
});
