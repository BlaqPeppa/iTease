/*import { clearInterval } from "timers";*/
/*jslint node: true */
/*globals $,alert,window,document,history,FormData,Spinner */
"use strict";
var app = {
	ModalWindow:function(title, subtitle, icon, url){
		var self = this;
		var onOpenCBs = [];
		this.setZIndex = function(v){
			$(this.element).iziModal('setZindex', v);
		};
		if (url) this.onOpen = function(modal){
			modal.startLoading();
			$.get(url, function(data) {
				$("#modal .iziModal-content").html(data);
				modal.stopLoading();
			});
		};
		else this.onOpen = function(modal){ };
		var html = '<div class="modal';
		if (title) html += ' data-iziModal-title="'+title+'"';
		if (subtitle) html += ' data-iziModal-subtitle="'+subtitle+'"';
		if (icon) html += ' data-iziModal-icon="'+icon+'"';
		html += '></div>';
		self.element = $('<div class="modal"></div>').appendTo('body');
		self.element.iziModal({
			onOpening:this.onOpen
		});
	},
	setup:null,
	ready:false,
	bInit:false,
	docLoad:false,
	transitions:['fade', 'slide'],
	is_modal:false,
	base_element:null,
	alert:function(msg){
		alert(msg);
	},
	refresh_interval:{},
	update_callbacks:[],
	update:function(fn){
		this.update_callbacks.push(fn);
	},
	update_event:function(o, data){
		for (var i=0; i<this.update_callbacks.length; ++i) {
			this.update_callbacks[i](o, data);
		}
	},
	createModalWindow:function(title, subtitle, icon, url){
		var mw = new this.ModalWindow(title, subtitle, icon, url);
		mw.setZIndex(10000);
		return mw;
	},
	transition:function(obj, opts, content){
		var ani = 'fade';
		var ani_in = '';
		var ani_out = '';
		var dur = 600;
		var dur_in = 0;
		var dur_out = 0;
		var delay = dur / 2;
		if (typeof(opts.transition) !== 'undefined' && this.transitions.indexOf(opts.transition) != -1)
			ani = opts.transition;
		if (typeof(opts.transition_in) !== 'undefined' && this.transitions.indexOf(opts.transition_in) != -1)
			ani_in = opts.transition_in;
		if (typeof(opts.transition_out) !== 'undefined' && this.transitions.indexOf(opts.transition_out) != -1)
			ani_out = opts.transition_out;
		if (typeof(opts.transition_time) !== 'undefined')
			dur = opts.transition_time;
		if (typeof(opts.transition_in_time) !== 'undefined')
			dur_in = opts.transition_in_time;
		if (typeof(opts.transition_out_time) !== 'undefined')
			dur_out = opts.transition_out_time;
		if (typeof(opts.transition_delay) !== 'undefined')
			delay = opts.transition_delay;
		
		if (!ani_in) ani_in = ani;
		if (!ani_out) ani_out = ani;
		if (!dur_in) dur_in = dur;
		if (!dur_out) dur_out = dur;
		var $obj = $(obj);
		var in_func = function(){
			$obj.html(content);
			setTimeout(function(){  $obj.show(); }, delay);
		};
		if (ani_in == 'fade') {
			in_func = function(){
				$obj.html(content);
				setTimeout(function(){  $obj.fadeIn(dur_in); }, delay);
			};
		}
		else if (ani_in == 'slide') {
			in_func = function(){
				$obj.html(content);
				setTimeout(function(){  $obj.slideDown(); }, delay);
			};
		}
		if (ani_out == 'fade')
			$obj.fadeOut(dur_out, in_func);
		else if (ani_out == 'slide')
			$obj.slideUp(dur_out, in_func);
	},
	pageUpdate:function(){

	},
	handleAjaxResponse:function(obj, data){
		var success = true;
		var $obj = $(obj);
		$obj.find('.ajax-error,.ajax-success').remove();
		$obj.find('.form-group').removeClass('error');
		if ($obj.is('form') && typeof(data.opts) !== 'undefined' && typeof(data.opts.path) !== 'undefined') {
			var newobj = $('.block_'+data.opts.path.split('/').join('_'));
			if (newobj.length) $obj = newobj;
		}
		if (typeof(data.errors) !== 'undefined') {
			var el;
			if (typeof data.errors == 'object') {
				for (var k in data.errors) {
					var input = $obj.find(':input[name="'+k+'"]');
					el = input.parentsUntil($obj, ".form-group");
					if (el.length) {
						el.addClass('error');
						var ar = el.find('.ajax-result');
						if (ar.length) el = ar;
						if (typeof data.errors[k] == 'string')
							el.append('<div class="ajax-error">' + data.errors[k] + '</div>');
						else if (data.errors[k] instanceof Array) {
							for (var i=0; i<data.errors[k].length; ++i) {
								el.append('<div class="ajax-error">' + data.errors[k][i] + '</div>');
							}
						}
					}
					success = false;
				}
			}
			else if (typeof data.errors == 'string') {
				el = $obj;
				if (el.data('button') && el.data('button').length) el = $(el.data('button'));
				el.prepend('<div class="ajax-error">'+data.errors+'</div>');
			}
		}
		if (typeof(data.content) !== 'undefined') {
			var $new_obj = $(data.content);
			if (typeof(data.opts.page_style) !== 'undefined') {
				$('body').removeClass('modal').removeClass('full');
				if (data.opts.page_style == 'modal')
					$('body').addClass('modal');
				else
					$('body').addClass('full');
			}
			if (typeof(data.opts.transition) !== 'undefined' || (
				typeof(data.opts.transition_in) !== 'undefined' && typeof(data.opts.transition_out) !== 'undefined'
			)) {
				this.transition(obj, data.opts, $new_obj);
				var time = typeof(data.opts.transition_in) !== 'undefined' ?
					data.opts.transition_in :
					(typeof(data.opts.transition) !== 'undefined' ? data.opts.transition : 600);
			}
			else $obj.html($new_obj);
			this.updater($new_obj, data);
		}
		else this.updater($obj, data);
		return success;
	},
	updaters:{},
	update_ui:function(o, ui){
		var updates = typeof(ui.update) !== 'undefined' ? ui.update : [];
		if (updates.length) {
			for (var i=0; i<updates.length; ++i) {
				var update = updates[i];
				var obj = o;
				if (typeof(update.selector) !== 'undefined' && update.selector)
					o = $(update.selector);
				this.updaters[update.selector] = o;
			}
		}
	},
	update_page:function(){
		var self = this;
		if (Object.keys(this.updaters).length) {
			$.ajax({
				method: 'GET',
				url: '/updates',
				contentType: false,
				processData: false,
				dataType: 'json'
			}).done(function(data){
				if (typeof(data.updates) !== 'undefined') {
					for (var i=0; i<data.updates.length; ++i) {
						if (typeof(data.updates[i].content) === 'undefined') continue;
						if (data.updates[i].selector in self.updaters) {
							self.updaters[data.updates[i].selector].html(data.updates[i].content);
							if (data.updates[i].complete)
								delete self.updaters[data.updates[i].selector];
						}
					}
				}
			});
		}
		setTimeout((function(){ return function(){ self.update_page.call(self); }; })(), 1000);
	},
	reset_page:function(){
		this.updaters = {};
		if (Object.keys(this.refresh_interval).length) {
			for (var k in this.refresh_interval) {
				clearInterval(this.refresh_interval[k]);
			}
		}
	},
	updater:function(o, data){
		var self = this;
		var opts = {};
		var $o = $(o);
		if (typeof(data) !== 'undefined') {
			if (typeof(data.opts) !== 'undefined')
				opts = data.opts;
			if (typeof(opts.title) !== 'undefined' && typeof(opts.url) !== 'undefined') {
				history.pushState(opts, opts.title, opts.url);
				this.reset_page();
			}
			if (typeof(data.update) !== 'undefined' && data.update.length)
				this.update_ui(o, data);
		}
		else {
			$.ajax({
				method: 'GET',
				url: '/update',
				contentType: false,
				processData: false,
				dataType: 'json'
			}).done(function(data){
				if (typeof(data.update) !== 'undefined' && data.update.length)
					self.update_ui(o, data);
			});
		}

		$o.data('opts', opts);
		if (typeof(opts.path) !== 'undefined') $o.data('path', opts.path);
		$o.off('click', 'a');
		$o.on('click', 'a', function(){
			var url = $(this).attr('href');
			$.ajax({
				method: 'GET',
				url: url,
				contentType: false,
				processData: false,
				dataType: 'json'
			}).done(function(data){
				console.log(data);
				self.handleAjaxResponse(self.base_element, data);
			});
			return false;
		});
		if (typeof(opts.form_success) !== 'undefined') {
			$o.find('form').data('on_success', opts.form_success);
		}

		$o.find('.date').each(function(){
			var format = 'yy-mm-dd';
			var dob = false;
			if ($(this).data('date-dob') || $(this).hasClass('date-dob')) dob = true;
			if ($(this).data('date-format')) format = $(this).data('date-format');
			var opts = {
				dateFormat:format,
				changeMonth:true,
				changeYear:true
			};
			if (dob) {
				opts.yearRange = '-117:';
				opts.maxDate = '-18Y';
			}
			$(this).datepicker(opts);
		});
		$o.off('keypress click', 'form input[type="submit"],form button[type="submit"]');
		$o.on('keypress click', 'form input[type="submit"],form button[type="submit"]', function(e){
			if (e.which === 13 || e.type === 'click')
				$(e.target).parents('form').data('button', e.target);
		});
		$o.off('submit', 'form');
		$o.on('submit', 'form', function(){
			var form = $(this);
			var opts = $(form).data('opts');
			var on_success = $(form).data('on_success');
			
			if (form.data('submission-active')) return false;
			form.data('submission-active', true);
			var fd = new FormData(this);
			var btn = $(form.data('button'));
			if (btn.length) fd.append(btn.attr('name'), btn.val());

			var url = $(form).attr('action');
			var XHRs = [];
			var pass_fail = false;
			var passwords = {};

			if (url !== '/signin' && url !== '/signup') {
				form.find(':input[type="password"]').each(function(){
					var pf = $(this);
					passwords[pf.attr('name')] = pf.val();
				});
				if (typeof(opts.password) === 'string' && typeof(opts.password_confirm) === 'string') {
					if (typeof(passwords[opts.password]) !== 'undefined' && typeof(passwords[opts.password_confirm]) !== 'undefined') {
						var errors = {};
						if (passwords[opts.password].length < 6) {
							pass_fail = true;
							errors[opts.password] = "Minimum password length: 6 characters";
						}
						else if (passwords[opts.password].length > 100) {
							pass_fail = true;
							errors[opts.password] = "Maximum password length: 100 characters";
						}
						else if (passwords[opts.password] !== passwords[opts.password_confirm]) {
							pass_fail = true;
							errors[opts.password] = "Passwords do not match";
							errors[opts.password_confirm] = "Passwords do not match";
						}
						if (pass_fail) self.handleAjaxResponse(form, {errors:errors});
					}
				}
				if (!$.isEmptyObject(passwords) && !pass_fail) {
					XHRs.push($.ajax({
						method: 'post',
						url:'/_password.json',
						data: passwords,
						dataType: 'json'
					}).done(function(data){
						if (typeof(data) === 'object' && Object.keys(data).length == Object.keys(passwords).length) {
							var keys = fd.keys();
							for (var k in data) {
								if (!passwords.hasOwnProperty(k)) {
									pass_fail = true;
									break;
								}
								fd.set(k, data[k]);
							}
						}
						else pass_fail = true;
					}).fail(function(jqXHR, error, errorThrown){
						pass_fail = true;
						console.log(jqXHR);
						console.log(error);
						console.log(errorThrown);
					}));
				}
			}
			
			$.when.apply(undefined, XHRs).then(function(){
				if (pass_fail) {
					alert('Failed to secure password, please try again');
					form.data('submission-active', false);
					return;
				}
				$(form).find('.form-group').removeClass('error');
				if (btn.length) {
					btn.prop('disabled', true);
					btn.removeClass('loading').addClass('loading');
					btn.data('original-text', btn.html());
					btn.data('Loading...');
					//btn.append('<img src="/image/spinner.gif" alt="">');
				}
				$.ajax({
					method: $(form).attr('method'),
					url: url,
					data: fd,
					contentType: false,
					processData: false,
					dataType: 'json'
				}).done(function(data){
					console.log(data);
					if (self.handleAjaxResponse(form, data)) {
						if (on_success) {
							$.ajax({
								method: 'GET',
								url: on_success,
								data: data,
								contentType: false,
								processData: false,
								dataType: 'json'
							}).done(function(data){
								console.log(data);
								self.handleAjaxResponse(form, data);
							}).fail(function(jqXHR, error, errorThrown){
								console.log(jqXHR);
								console.log(error);
								console.log(errorThrown);
							});
						}
					}
				}).fail(function(jqXHR, error, errorThrown){
					console.log(jqXHR);
					console.log(error);
					console.log(errorThrown);
				}).always(function(){
					btn.removeClass('loading');
					btn.prop('disabled', false);
					btn.html(btn.data('original-text'));
					form.data('submission-active', false);
				});
			});
			return false;
		});
		var doDataUpdateSetup = function(){
			if ($(this).data('has-updater')) return;
			$(this).data('has-updater', true);
			var delay = parseInt($(this).data('update-delay'));
			var interval = parseInt($(this).data('update-interval'));
			if (isNaN(delay) && !isNaN(interval)) delay = interval;
			var doUpdate = function(obj){
				var path = $(obj).data('update');
				var modal = $(obj).parents('body.modal>#base>main');
				$.ajax({
					method: 'GET',
					url: path,
					dataType: 'json'
				}).done(function(data) {
					self.handleAjaxResponse(obj, data);
				}).fail(function (jqXHR, error, errorThrown) {
					console.log(jqXHR);
					console.log(error);
					console.log(errorThrown);
				}).always(function(){
					if ($(obj).length && !isNaN(interval))
						setTimeout((function(obj){ return function(){ return doUpdate(obj); }; })(obj), interval);
				});
			};
			if (!isNaN(delay)) setTimeout((function(obj){ return function(){ return doUpdate(obj); }; })(this), delay);
			else doUpdate(this);
		};
		if ($o.data('update')) {
			if ($o.length > 1) $o.each(doDataUpdateSetup);
			else doDataUpdateSetup.call(o);
		}
		$o.find('[data-update]').each(doDataUpdateSetup);
		this.update_event(o, data);
	},
	init:function(){
		$('body').data('block', 'body');
		this.base_element = $('body.modal>#base');
		if (this.base_element.length)
			this.is_modal = true;
		else
			this.base_element = $('#base');
		this.base_element.on('click', '[data-tab]', function(){
			var tabs = $(this).parents('.tabs');
			var tab = $(this).data('tab');
			tabs.find('[data-tab]').removeClass('active');
			$(this).addClass('active');
			$(tabs).find('.tab').hide();
			$(tabs).find(tab).show();
			return false;
		});
		this.updater(this.base_element);
		var self = this;
		setTimeout((function(){ return function(){ self.update_page.call(self); }; })(), 100);
	},
	doSetup:function(){
		alert('hi');
		if (typeof this.setup.firstrun === "undefined") {
			if (window.location !== "setup") {
				window.location = "/setup";
			}
		}
		this.ready = true;
		this.init();
	},
	_docReady:function(){
		this.docLoad = true;
		this._init();
	},
	_init:function(){
		if (this.docLoad) this.init();
	},
};
(function(){
	app._init();
	$(document).ready(function(){app._docReady();});
})();