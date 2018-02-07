/** iTease App Module Script **/
// The following comment tells JSHint that certain variables are already defined
/*globals module, require, print */
/**
 * The result of the following self-executing function will be stored to 'exports'
**/
module.exports = (function(){
	'use strict';
	/**
	 * Require needed core modules
	**/
	var web = require('web');
	var users = require('users');
	var fs = require('filesystem');
	
	/**
	 * The init_done variable will help prevent the init() function from running after startup
	**/
	var init_done = false;
	
	/**
	 * An initialisation function. For demonstration purposes this is called 'init'.
	 * Note that all occurences of 'init' in this script can be replaced with 'start' without breakings
	**/
	var init = function() {
		/**
		 * Here 'this' will refer to the 'app' object we're exporting
		 * However 'this' may change in a function, so save it to another variable
		**/
		var app = this;
		
		/**
		 * If init_done is true, return, as we've already initialised
		 * Otherwise, set init_done to true!
		**/
		if (init_done) return;
		else init_done = true;
		
		/**
		 * Add a controller which will manage the front-end display of web templates...
		 * 	See 'system/controller/main.js'
		 * Assign it to this.controller, so we can access it via require('app').controller
		**/
		this.controller = require('./controller');
		
		/**
		 * With 'addPage' in the controller we can set up a page and its data.
		**/
		this.setup_page = this.controller.addPage('startup');
		this.main_page = this.controller.addPage('main');
		this.chat_page = this.controller.addPage('chat');
		this.library_page = this.controller.addPage('library');
		this.exit_page = this.controller.addPage('exit');
		this.setup_page.title = 'iTease';
		this.controller.setPage('main');
		this.controller.navigation = [
			{url:'/', text:'Home'},
			{url:'/chat', text:'Chat'},
			{url:'/tease', text:'Tease'},
			{url:'/library', text:'Library'},
			{url:'/exit', text:'Exit'},
		];
		
		/**
		 * Adds a callback handler for web requests, which is called any time a web browser asks
		 * for a file or information. By using this we can obtain information about what the
		 * browser wants and customise our future output accordingly.
		**/
		web.listen(function(request){
			// Check if there exists a static file in the web directory for the requested URI
			if (web.isStaticFile(request.uri)) {
				// Use web.createStaticResponse to return the static file to the web browser
				return web.createStaticResponse(request.uri);
			}
			// if user is not logged in, refer to startup page
			if (typeof(request.session.user) === 'undefined')
				app.controller.setPage('startup');
			// logged in, so refer from the startup page to the main page
			else if (app.controller.getPage().id == 'startup')
				app.controller.setPage('main');
			// if the request has a better idea, entertain it
			else {
				var uri = request.uri;
				if (uri[0] == '/') uri = uri.substr(1);
				// the request doesn't know we call the main page 'main', so set it if it didn't request another page
				// however if it requested 'main', unset it, coz 'magicians code'...
				if (uri == 'main') uri = '';
				else if (!uri.length) uri = 'main';
				// if the request is asking for a page that exists, respond with that
				if (app.controller.hasPage(uri))
					app.controller.setPage(uri);
			}
			return app.controller;
		});
	};
	
	/**
	 * This trickery is equivalent to writing "exports = {..."
	 * However we can write code above, within our self-executing function to help
	 * with the setup of this structure.
	 * So the init() function declared above is invisible outside of this block in this file.
	**/
	return {
		/**
		 * Return an object to write to exports. So declaring the 'start' property here is
		 * equivalent to writing 'exports.start = ...' anywhere. These will be visible to our caller.
		 * So require('./app').start() will call the 'init' function, while 'init' remains invisible.
		**/
		start: init
	};
})();