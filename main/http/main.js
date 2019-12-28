/*=======================================================================*/
/* DOM MODIFICATION HELPERS */
function el(x) {
	return document.getElementById(x);
}

function hide(x, h) {
	var e = el(x);
	if (e) e.style.display = (h)?"none":"";
}

function dis(x, d) {
	el(x).disabled = d;
}

/*=======================================================================*/
/* XMLHTTPREQUEST HELPERS */
function str_ld(fn, onload) {
	var req = new XMLHttpRequest();
	req.onreadystatechange = function() {
		if (req.readyState != 4) return;
		if (onload) {
			if (req.status == 200) {
				onload(req.responseText);
			} else {
				onload(null);
			}
		}
	};
	req.open('GET', fn, true);
	req.send();
}

function bin_ld(fn, onload, onprg) {
	var req = new XMLHttpRequest();
	req.onreadystatechange = function() {
		if (req.readyState != 4) return;
		if (onload) {
			if (req.status == 200) {
				onload(new Uint8Array(req.response));
			} else {
				onload(null);
			}
		}
	};
	if (onprg)
		req.addEventListener("progress", onprg, false);
	req.open('GET', fn, true);
	req.responseType = "arraybuffer";
	req.send();
}

function bin_st(fn, data, ondone, onprg, method) {
	var req = new XMLHttpRequest();
	req.onreadystatechange = function() {
		if (req.readyState != 4) return;
		if (ondone) {
			if (req.status == 200) {
				ondone(true);
			} else {
				ondone(false);
			}
		}
	};
	if (onprg)
		req.upload.addEventListener("progress", onprg, false);
	if (!method) method = 'PUT';
	req.open(method, fn, true);
	req.send(data);
}

function bin_del(fn, ondone) {
	var req = new XMLHttpRequest();
	req.onreadystatechange = function() {
		if (req.readyState != 4) return;
		if (ondone) {
			if (req.status == 200) {
				ondone(true);
			} else {
				ondone(false);
			}
		}
	};
	req.open('DELETE', fn, true);
	req.send();
}


/*=======================================================================*/
/* STATUS MESSAGE */
var sts_timeout;

function sts_set(str, cl) {
	var st = document.getElementById('st');
	st.innerHTML = str;
	st.className = cl;
	st.style.display = (str == "")?"none":"";
	if (sts_timeout) {
		clearTimeout(sts_timeout);
		sts_timeout = null;
	}
	if (str != "") sts_timeout = setTimeout(sts_del, 5000);
}

function sts_del() {
	sts_set("","")
}
function sts_ok(str) {
	sts_set(str,"st-ok")
}
function sts_warn(str) {
	sts_set(str,"st-warn")
}
function sts_err(str) {
	sts_set(str,"st-err")
}
function sts_busy(str) {
	sts_set(str,"st-busy")
}
