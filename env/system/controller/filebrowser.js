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
	var fs = require('filesystem');
	//
	var i = 0;
	page.url = '/filebrowser';
	page.title = 'File Browser';
	return {
		onRequest:function(request, block){
            print('File Browser');
            print(request);
            print(block);
			return;
		}
	};
};