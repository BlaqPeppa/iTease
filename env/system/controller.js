/** iTease main Web Controller **/
/**
 * This script controls how template files are taken and turned into HTML with dynamic content.
 * Actually, it doesn't have to be HTML, templating works with any type of text file.
 * The exported module will be a web.Controller object with additional functions and handlers from this file.
**/
// The comment below allows JSHint to know module and require are already declared as global variables.
/*globals module, require */
/**
 * The result of the following self-executing function will be stored to 'exports'
**/
module.exports = (function(){
	'use strict';
	
	var web = require('web');
	var fs = require('filesystem');
	
	/**
	 * web.TemplateFile is a constructor function - it creates an object.
	 * You must use 'new' before the call to it (because tradition).
	 * The web.TemplateFile function accepts 1 or 2 arguments.
	 * The first argument must be the path to a template file.
	 * The optional second argument (an object) can define template variable and block values.
	 * See the template file for more insight.
	**/
	var stylesheet = new web.TemplateFile('system/template/css/page.css', {
		vars: {
			base_bg_color:'#3F2440',
			bg_color:'#B882B9',
			//link_color:'#FF2626',
			link_color:'#FF5555',
			link_hover_color:'#F1A899;',
			max_width:'1100',
		}
	});
	var template = new web.TemplateFile('system/template/html/page.html', {
		/**
		 * Variables, such as this one, the title of the webpage, may need to
		 * change often. Here we're basically setting the initial/default values
		 * and we'll manage updating them in the 'onRender' callback function.
		**/
		vars: {
			/**
			 * Vars can be set to strings here. The variable reference in the template
			 * will be replaced with the value of the variable that we set as so:
			 *	<title>{%var:title%}</title>
			 * ->
			 *	<title>iTease WebUI</title>
			**/
			'title':'iTease WebUI',
			'version_major':iTease.version.major,
			'version_minor':iTease.version.minor
		},
		blocks: {
			/**
			 * Like vars, blocks can be set to strings. However they can also be set
			 * to other template files.
			 * Note that these "child" blocks will be given the variables of the current block,
			 * so 'title' within any of these blocks starts off as "iTease WebUI".
			 * The variables in these new blocks can be changed independently.
			**/
			meta:new web.TemplateFile('system/template/html/page/meta.html', {
				/**
				 * As above we can set the initial values of blocks and vars
				 * If we pass an array for the vars, the block will repeat over with the specified values in
				 * each element, similar to using .setArray([...])
				**/
				blocks: {
					vars:{
						title:'iTease 1.0'
					},
					http:{
						vars:[
							{name:'Content-Type', value:'text/html; charset=utf-8'},
							{name:'X-UA-Compatible', value:'IE=8,IE=9,IE=10'}
						]
					},
					standard:{
						vars:[
							{name:'viewport', value:'width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=0'}
						]
					}
				}
			}),
			icons:'',
			css:new web.TemplateFile('system/template/html/page/css.html', {
				blocks: {
					stylesheet: {
						vars:[
							//{source: '/css/fonts.css'},
							/**
							 * web.TemplateFile objects have a 'cacheFile' method which will output a
							 * managed cache file and return the resulting path in the web directory.
							 * The two strings provided specify the file path prefix and suffix, which
							 * is concatenated to a randomly generated hash for a unique file path.
							**/
							{source: stylesheet.cacheFile("css/", ".css")},
							{source: '/css/jquery/jquery-ui.min.css'},
							{source: '/css/jquery/jquery-ui.theme.min.css'}
						]
					}
				}
			}),
			body: {
				blocks: {
					nav:new web.TemplateFile('system/template/html/page/nav.html')
				}
			},
			body_attr:{
				vars:[]
			},
			javascript_top:new web.TemplateFile('system/template/html/page/javascript.html', {
				vars: [
				]
			}),
			javascript:new web.TemplateFile('system/template/html/page/javascript.html', {
				vars: [
					{source:"/javascript/jquery/jquery.min.js"},
					{source:"/javascript/jquery/jquery-ui.min.js"},
					{source:"/javascript/imagesloaded.min.js"},
					{source:"/javascript/spin.min.js"},
					{source:"/javascript/iziModal.min.js"},
					{source:"/javascript/app.js"},
					{source:"/javascript/common.js"}
				]
			})
		}
	});

	var controller;
	
	/**
	 * Creates a Page instance for page management within this controller
	 * @constructor
	 * @param {string} page_id	The desired ID of the page
	**/
	function Page(page_id) {
		// A page will have an ID to identify it by, as well as some meta info (e.g. title, URL)
		// Having this information here will make it easier to perform certain tasks later
		this.id = page_id;
		this.title = '';
		this.url = '/';
		this.opts = {};
		
		// Attempt to auto-load a main template file for this page
		this.template = fs.isFile('system/template/html/'+page_id+'.html') ?
							new web.TemplateFile('system/template/html/'+page_id+'.html') :
							null;
		// Get the template {%opt%} values and include them, allowing page options to be specified within the template
		if (this.template) this.opts = this.template.opts;

		if (typeof(this.opts.page_style) === 'undefined' || !this.opts.page_style)
			this.opts.page_style = 'full';

		// Check for a page controller
		this.module = null;
		if (fs.isFile('system/controller/'+page_id+'.js'))
			this.module = require('controller/'+page_id)(this, controller);
	}
	
	// Default the page to a blank page
	var page = new Page('blank');
	var pages = {};
	var self = this;

	// The main aim of this file is to return a web.Controller
	// We can extend the type to provide more advanced controller functionality
	controller = new web.Controller(template);
	controller.Page = Page;
	controller.Updater = Updater;

	controller.navigation = [];
	controller.updates = [];

	function Updater(selector) {
		var self = this;
		this.selector = selector;
		this.content = '';
		this.is_complete = false;
		controller.updates.push(this);
		return {
			update:function(content){
				self.content = content;
			},
			complete:function(content){
				if (content) this.update(content);
				self.is_complete = true;
			}
		};
	}
	
	/**
	 * Add a page to this controller
	 * @this {web.Controller}
	 * @param 	{string} page_id	a page ID
	 * @return 	{Page}				the new page
	**/
	controller.addPage = function(page_id) {
		pages[page_id] = new Page(page_id);
		return pages[page_id];
	};
	/**
	 * Check if this controller has a page with the requested ID
	 * @this {web.Controller}
	 * @param 	{string} page_id	a page ID
	 * @return 	{bool}				true if the page exists, else false
	**/
	controller.hasPage = function(page_id) {
		return typeof pages[page_id] !== 'undefined';
	};
	/**
	 * Get a page with the requested page ID
	 * @this {web.Controller}
	 * @param 	{string} [page_id]	a page ID, returns current page if excluded
	 * @return 	{(Page|null)}		the page, or null if the page does not exist
	**/
	controller.getPage = function(page_id) {
		if (typeof(page_id) === 'undefined') return page;
		return typeof pages[page_id] !== 'undefined' ? pages[page_id] : null;
	};
	/**
	 * Load a page into the controller but don't set it as the active page
	 * @this {web.Controller}
	 * @param 	string	a page ID
	 * @return 	bool	true on success, false if the page id does not exist
	**/
	controller.loadPage = function(page_id) {
		if (typeof pages[page_id] === 'undefined')
			return false;
		page = pages[page_id];
		/**
		 * Setup the template for the page	
		 * If the page has an associated template, load it into the body
		**/
		this.template.meta.vars.title = page.title ? ('iTease :: '+page.title) : 'iTease Web UI';
		this.template.body.vars.page_id = page.id;
		this.template.opts.url = page.url;
		this.template.opts.title = page.title;
		this.template.opts.path = this.template.path;
		this.template.opts.page_style = page.opts.page_style;
		if (typeof this.template.body.nav.item !== 'undefined') {
			var vars = [];
			vars = this.navigation;
			for (var i=0; i<vars.length; ++i) {
				vars[i].active = page.url == vars[i].url;
			}
			this.template.body.nav.item.vars = vars;
		}
			
		if (page.template) {
			/*if (page.template.before_main) {
				if (!this.template.body.before_content)
					this.template.body.insertBlock('before_content');
				this.template.body.before_content = page.template.before_main;
				page.template.before_main.enabled = false;
			}
			if (page.template.after_main) {
				if (!this.template.body.after_content)
					this.template.body.addBlock('after_content');
				this.template.body.after_content = page.template.after_main;
			}*/
			var page_style = 'full';
			if (page.opts.page_style) page_style = page.opts.page_style;
			else page.opts.page_style = page_style;
			this.template.vars.page_style = page_style;
			this.template.opts.page_style = page_style;
			// on full pages, enable the header/footer/navigation
			this.template.body.header.enabled = page_style == 'full';
			this.template.body.footer.enabled = page_style == 'full';
			this.template.body.nav.enabled = page_style == 'full';

			this.template.body.content.enabled = true;
			this.template.body.content = page.template;
		}
		return true;
	};
	/**
	 * Set the current active page to be shown by this controller
	 * @this {web.Controller}
	 * @param 	string	a page ID
	 * @return 	bool	true on success, false if the page id does not exist
	**/
	controller.setPage = function(page_id) {
		if (controller.loadPage(page_id)) {
			page = pages[page_id];
			return true;
		}
		return false;
	};

	controller._getUpdates = function(request) {
		var updates = [];
		var remaining = [];
		for (var i=0; i<this.updates.length; ++i) {
			var u = this.updates[i];
			updates.push({
				selector:u.selector,
				content:u.content,
				complete:u.is_complete
			});
			if (!u.is_complete)
				remaining.push(u);
		}
		this.updates = remaining;
		return updates;
	};
	controller._prepareUpdates = function(){
		var updates = [];
		for (var i=0; i<this.updates.length; ++i) {
			var u = this.updates[i];
			updates.push({
				selector:u.selector,
				complete:u.is_complete
			});
		}
		return updates;
	};
	controller._getPageResponse = function(request) {
		// Re-cache the stylesheet
		/*stylesheet.vars.base_bg_color = typeof(request.params.colour) !== 'undefined' ? request.params.colour : 'white';
		if (template.css.stylesheet.vars.source)
			stylesheet.cacheFile(template.css.stylesheet.vars.source.substr(7));
		else
			template.css.stylesheet.vars.source = stylesheet.cacheFile("css/", ".css");
		print(template.css.stylesheet.vars.source);*/
		
		// Add an 'is_ajax' boolean property for convenience, the following conditions verify the request was sent via AJAX
		request.is_ajax = typeof(request.headers['X-Requested-With']) !== 'undefined' &&
						  request.headers['X-Requested-With'].toLowerCase() == 'xmlhttprequest';
		request.is_post = request.method.toLowerCase == "post";
		var ajax = request.is_ajax;
		
		// Split up the URI path into an array
		var arr = request.uri.split('/');
		var page, res;
		var page_request = false;
		
		// If the URI path begins with a / then the first array element will be empty, remove that
		if (arr.length && !arr[0]) arr.shift();
		
		// Check there is more to the URI path and if the first part is 'page' (/page/page_id/etc...), check for a matching page
		if (arr.length >= 1) {
			if (arr.length > 1 && arr[0] == 'page') {
				// Attempt to get a page using the second part of the URI path as a page ID
				if (this.loadPage(arr[1])) {
					page = this.getPage(arr[1]);
					page_request = true;
				}
			}
			// If the first part of the URI is 'update', return any updates for the page
			else if (arr[0] == 'update' && ajax) {
				return {update: this._prepareUpdates()};
			}
			else if (arr[0] == 'updates' && ajax)
				return {updates: this._getUpdates(request)};
		}
		
		if (!page) {
			// If no page is specified in the URI, process the current active page
			page = this.getPage();
			this.loadPage(page.id);
		}

		if (page) {
			// The page might not already have a template, but may have a module or be intentionally blank
			if (page.template) {
				// Note that if we copy page.template rather than template.body.content, it won't have the
				// full path we may expect, the act of assigning page.template to a block sets the path
				this.template.body.content = page.template;
				this.template.body.vars.page_id = page.id;
				var template;
				if (page_request) {
					template = arr.length > 2 ? this.template.body.content : this.template.body;
					// If there is more to the request URI, traverse for sub-blocks matching the path
					for (var i=2; i<arr.length; ++i) {
						if (template.blocks.indexOf(arr[i]) != -1)
							template = template[arr[i]];
					}
				}
				else template = this.template.body;
				template.opts.url = page.url;
				template.opts.title = page.title;
				template.opts.path = template.path;
				template.opts.page_style = page.opts.style;
				// Call any request handler for the page so it may process stuff...
				if (page_request) {
					if (page.module && typeof(page.module.onRequest) === 'function') {
						// If the handler returns a value, use it as the response
						res = page.module.onRequest(request, template);
						if (typeof(res) !== 'undefined') return Object.assign(res, {'update': this._prepareUpdates()});
					}
					template.opts.url = page.url;
					template.opts.title = page.title;
					template.opts.path = template.path;
					template.opts.page_style = page.opts.page_style;
					// If it's an ajax request, we'll return a JSON result containing the template and template options (for awesomeness)
					// This allows the front-end to update without re-loading the entire page
					if (!ajax)
						return template;
					return {
						update: this._prepareUpdates(),
						opts: template.opts,
						content: template.render()
					};
				}
			}
			if (page_request) {
				// If the page has a module with an 'onRequest' handler, call it for the response, or return a blank one
				if (page.module && typeof(page.module.onRequest) === 'function') {
					res = page.module.onRequest(request);
					if (typeof(res) !== 'undefined') return Object.assign(res, {'update': this._prepareUpdates()});
				}
				return '';
			}
		}

		if (page.module && typeof(page.module.onRequest) !== 'undefined') {
			// If there is a module, let it do its magic on the template block
			res = page.module.onRequest(request, this.template.body.content);
			// If the module returned something, it will be a response we can return
			if (typeof(res) !== 'undefined') return Object.assign(res, {'update': this._prepareUpdates()});
		}
		this.template.opts.url = page.url;
		this.template.opts.title = page.title;
		this.template.opts.path = this.template.path;
		if (ajax) {
			return {
				update: this._prepareUpdates(),
				opts: this.template.opts,
				content: this.template.body.render()
			};
		}
		// The module may have altered this.template via onRequest, or this may be a static template to return...
		return this.template;
	};
	
	/**
	 * controller.update can be used to issue update commands to the front-end
	 * @param string	A selector to the DOM element to update
	 * @param string	The contents to update the DOM element with
	**/
	controller.update = function(selector, content) {
		this.updates.push({
			selector: selector,
			content: content
		});
	};

	/**
	 * controller.onRequest is called if the controller is returned from web.listen
	 * @param object	An object containing the request parameters
	 * @return mixed	Any kind of response to be rendered, including strings, JavaScript objects (rendered as JSON),
	 *					templates and template blocks.
	**/
	controller.onRequest = function(request){
		var response = this._getPageResponse(request);
		//print(response);
		return response;
	};
	
	/**
	 * web.Controller prototype has an 'onRenderBlock' method which can be overriden with a
	 * function, as below. The function is passed the block being rendered within the template.
	 * The block can be modified here in order to alter variables and blocks dynamically.
	 * @this {web.Controller}
	 * @param {web.TemplateBlock} block	- the template block about to be rendered
	**/
	controller.onRenderBlock = function(block){
		/**
		 * The block 'path' variable allows us to identify the template block
		**/
		if (block.path == "body") {
			//print(block);
		}
	};
	
	// Return the controller object which may be used by caller to generate the page by returning it in web.listen()
	return controller;
})();