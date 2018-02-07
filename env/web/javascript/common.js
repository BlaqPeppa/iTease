var common = (function(){
    var common = this;
    this.loadFileBrowserModalContents = function(modal, path){
        modal.startLoading();
        $.ajax({
            type:'GET',
            url:'/filebrowser',
            data:{
                path:path
            }
        }).done(function(data) {
            $("#modal .iziModal-content").html(data);
            modal.stopLoading();
        });
    };
    app.update(function(o, data){ 
        $('.modal').iziModal();
        $(o).find(':input.browse').focus(function(){
            var self = $(this);
            var path = self.data('browse');
            var modal = app.createModalWindow('File Browser', null, null);
            modal.onOpen(function(im){
                common.loadFileBrowserModalContents(modal, path);
            });
        });
        $(o).find('.select-panel').each(function(){
            var self = $(this);
            self.find('.add-new').click(function(){
                self.find('.add-panel').slideDown();
            });
        });
        $(o).find('.edit-hyphenate').on('change', function(){
            $(this).val($(this).val().trim().split(' ').join('-').replace(/\-+/, '-'));
        });
    });
})();
/*(function($) {
    $.QSA = (function(a) {
        if (a == "") return {};
        var b = {};
        for (var i = 0; i < a.length; ++i)
        {
            var p=a[i].split('=', 2);
            if (p.length != 2) continue;
            b[p[0]] = decodeURIComponent(p[1].replace(/\+/g, " "));
        }
        return b;
    })(window.location.search.substr(1).split('&'))
})(jQuery);*/