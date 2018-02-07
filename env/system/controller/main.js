/**
 * system/template/html/main.html controller
 * @module page/main
 * @see module:controller
**/
/*globals module, require */
module.exports = function(page, controller){
	'use strict';
	var web = require('web');
	var users = require('users');
	//
	var i = 0;
	page.url = '/';
	page.title = '';
	return {
		onRequest:function(request, block){
			var opts = block.opts;
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