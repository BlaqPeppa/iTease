/**
 * system/template/html/startup.html controller
 * @module page/startup
 * @see module:controller
**/
/*globals module, require */
module.exports = function(page, controller){
	// 'git gud';
	'use strict';
	var web = require('web');
	var users = require('users');
	//
	var i = 0;
	page.url = '/startup';
	page.title = 'iTease Login';
	return {
		/**
		 * A template processing handler for web requests.
		 * Similar to, say, a PHP Script, but way more template oriented.
		 * Return a value to set output, otherwise the controller will default, most likely to the requested template/block.
		 * @param {Object} request 		the web request
		 * @param {Object} [block]		the requested template block, undefined if there's no associated template
		 * @return {*}
		**/
		onRequest:function(request, block){
			// This handler may be used for different requests, so check each one
			if (block.path == 'body/content') {
				// Main request for this page
				// Disable this block from showing, we'll send it separately as an AJAX response
				block.form.enabled = false;
				return;
			}
			// Good idea to verify requests for blocks are being sent via AJAX
			else if (block.path == 'body/content/form' && request.is_ajax) {
				// Requested by system/template/html/startup.html
				// enable/disable 'signin' segments based on whether there are any users to sign in
				if (!users.getNumUsers()) block.disable('signin');
				else block.enable('signin');
			}
			// no need to do anything else for signup/signin as iTease handles the /signup and /signin request automatically
			// but we do need to redirect the user once the login has been completed...
			else if (block.path == 'body/content/form/success') {
				controller.setPage('/');
				return;
			}
			return {
				'opts': block.opts,
				'content': block.render()
			};
		}
	};
};