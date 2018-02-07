/** iTease System Startup Script **/
/**
 * This is the entry point of the iTease EMCAScript (or 'JavaScript') Engine
 * It's only job is to run our main module called 'app' (found alongside this one)
 *
 * These core iTease scripts are commented excessively with explanations of the basic
 * EMCAscript practises in use to help users gain an understanding of how scripts are written.
 * For most users however these are no substitute for tutorials or guides, which shall
 * also be provided with distributions of iTease. Please consult those if you don't
 * understand what you are reading.
**/
// The following comment tells JSHint that certain variables are already defined
/*globals module, require, print */
/**
 * Wrap all code in self-executing invisible function to reduce clutter in this scope
 * e.g. declaring "var app" inside it allows 'app' to only be used within it
 * These are known as 'private variables' and are good for helping to declare public properties
**/
(function(){
	/**
	 * A 'directive' in JS is just a string with no operation.
	 * 'use strict' is a recommended directive to help enforce good script writing.
	 * It's best to use this at the function level
	**/
	'use strict';
	/**
	 * Print a message displaying iTease version and celebrating the fact this script is running
	**/
	print('iTease '+iTease.version.major+'.'+iTease.version.minor+' JavaScript Engine Started');
	
	/**
	 * 'require' the 'app' module by path (resolves to system/app.js)
	 * In 'require' calls './' refers to the current dir, '../' refers to the parent dir
	 * and '/' refers to the root dir - as with paths on your computer
	**/
	var app = require('./app');
	
	/**
	 * Run the module 'app'
	**/
	app.start();

	// tests...
	var html = require('html');
	var parser = html.parser;
	//var data = '<html><head><title>Test Testy Test Test Test</title></head><body><p class="classTest">Also a test</p></body></html>';
	//print(html);
	//var $ = parser.load(data);
	//print($);
	//print('aaaaa');
	//var title = $.find('title');
	//print(title);
})();
/*
var fs = require('filesystem');
var files = fs.ScanDir('plugins/*.js');
for (var i in files) {
	var plugin = require(files[i]);
	if (typeof(plugin.start) !== 'undefined')
		plugin.start();
}
*/