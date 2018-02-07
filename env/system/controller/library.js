/**
 * system/template/html/library.html controller
 * @module page/library
 * @see module:controller
**/
/*globals module, require */
module.exports = function(page, controller){
	'use strict';
	var web = require('web');
    var users = require('users');
	var net = require('net');
	var library = require('library');
	var html_module = require('html');
	var parser = html_module.parser;
	//
	var i = 0;
	page.url = '/library';
	page.title = 'Library';
	function setupCategoryPanel(block, mainBlock) {
		block.panels = mainBlock.category_panel;
		block.add_panel = mainBlock.add_category_panel;
	}
	return {
		onRequest:function(request, block){
			block.setup.enabled = true;
			block.category_panel.enabled = false;
			block.add_category_panel.enabled = false;
			setupCategoryPanel(block.setup.image_categories, block);
			setupCategoryPanel(block.setup.video_categories, block);
			/*var opts = block.opts;
			var req = net.get('https://google.com');
			var updater = new controller.Updater('.block_content');
			req.done(function(data, response){
				var $ = parser.load(data);
				var items = [];
				$.find('ul.videos>li').each(function(){
					var link = $(this).find('.title>a');
					if (!link.length) return true;
					items.push({
						text: link[0].textContent,
						url: ''
					});
				});
				var text = '';
				for (var i=0; i<items.length; i++) {
					text += '<a href="'+items[i].url+'">'+items[i].text+'</a>';
				}
				var dom = $.find('title');
				var title = dom[0].textContent;
				block.vars.response = text;
				updater.complete(block.render());
			});*/
			/*if (request.is_ajax) {
				return {
					'opts': opts,
					'content': block.render()
				};
			}*/
			return;
		}
	};
};