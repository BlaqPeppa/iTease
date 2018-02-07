function Web(module){
	'use strict';
	const self = this;
	var title = 'iTease';
	this.addController = module.addController;
	this.addPreController = module.addPreController;
	this.TemplateBlock = module.TemplateBlock;
	this.addPreController('/', function(template){
		template.body.header = new this.TemplateBlock('html/page/header');
		template.body.main = new this.TemplateBlock('html/page/main');
		template.body.footer = new this.TemplateBlock('html/page/footer');
	});
	this.addController('/', function(page){
		page.setVar('title', title);
		page.getBlock('');
		//if ()
	});
	/*this.setHandler = module.setHandler;
	this.pages = [];
	this.Template = function(path) {
		
	};
	function Page (path, fn){
		this.path = path;
		this.fn = fn;
		this.blocks = [];
		this.addBlock = function() {
			
		};
	};
	this.addPage = function(path, fn) {
		return module.addLocation(path, fn);
	};
	this.loadTemplate = function(path, fn){
		
	};*/
};