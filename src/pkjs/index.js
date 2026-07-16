var Clay = require('pebble-clay');
var buildConfig = require('./i18n').buildConfig;
var keys = require('message_keys');

// LANG stores the raw setting: '255' = Automatic (follow watch locale), or '0'..'4'.
function getLangRaw() { var v = localStorage.getItem('LANG'); return (v == null || v === '') ? '255' : v; }
// Language to render the settings page in (Automatic falls back to English).
function getPageLang() { var r = parseInt(getLangRaw(), 10); return (r >= 0 && r < 5) ? r : 0; }

// Clay is driven manually so the settings page can be rebuilt with the live
// project list (and language) each time it opens (autoHandleEvents:false).
var clay = new Clay(buildConfig(getPageLang(), null, { langRaw: getLangRaw() }), null,
  { autoHandleEvents: false });

// Todoist unified API v1 (the old REST v2 base returns HTTP 410 Gone).
var API_BASE = 'https://api.todoist.com/api/v1';

// Load states (mirror data.h).
var LOAD_LOADING = 0;
var LOAD_OK = 1;
var LOAD_ERROR = 2;
var LOAD_UNCONFIGURED = 3;

var MAX_PROJECTS = 16;
var MAX_TASKS = 64;
var TODAY_ID = 'today';

// In-memory cache + the list currently shown on the watch (for reloads).
var taskCache = {};
var current = { projectId: '' };

// --- Persisted phone-side state ---------------------------------------------

function getToken() { return (localStorage.getItem('TOKEN') || '').trim(); }

function getSel() {
  try { return JSON.parse(localStorage.getItem('SEL') || '[]'); }
  catch (e) { return []; }
}

function getAllProjects() {
  try { return JSON.parse(localStorage.getItem('ALL_PROJECTS') || '[]'); }
  catch (e) { return []; }
}

// Task-list text size: 0=small, 1=medium (default), 2=large (mirrors config.h).
function getFontSize() {
  var v = parseInt(localStorage.getItem('FONT_SIZE'), 10);
  return (v >= 0 && v <= 2) ? v : 1;
}

// Quick-complete: Select on a task completes it. Default off.
function getQuickComplete() { return localStorage.getItem('QUICK_COMPLETE') === '1'; }

function getStartView() { return parseInt(localStorage.getItem('START_VIEW') || '0', 10) || 0; }
function getDefId() { return localStorage.getItem('DEF_ID') || ''; }
function getDefName() { return localStorage.getItem('DEF_NAME') || ''; }

function selNameById(id) {
  var sel = getSel();
  for (var i = 0; i < sel.length; i++) { if (sel[i].id === id) return sel[i].name; }
  return '';
}

// --- HTTP (Todoist API v1, Bearer auth) -------------------------------------

// Generic request; cb(ok, status, data).
function request(method, path, body, cb) {
  var xhr = new XMLHttpRequest();
  xhr.open(method, API_BASE + path);
  xhr.setRequestHeader('Authorization', 'Bearer ' + getToken());
  if (body) { xhr.setRequestHeader('Content-Type', 'application/json'); }
  xhr.timeout = 15000;
  xhr.onload = function () {
    var ok = (xhr.status >= 200 && xhr.status < 300);
    var data = null;
    try { data = xhr.responseText ? JSON.parse(xhr.responseText) : null; } catch (e) {}
    if (!ok) { console.log(method + ' ' + path + ' -> ' + xhr.status); }
    cb(ok, xhr.status, data);
  };
  xhr.onerror = function () { console.log(method + ' failed ' + path); cb(false, 0, null); };
  xhr.ontimeout = function () { console.log(method + ' timeout ' + path); cb(false, 0, null); };
  xhr.send(body ? JSON.stringify(body) : null);
}

// v1 list endpoints may return a bare array or a { results: [...] } envelope.
function asList(data) {
  if (Array.isArray(data)) return data;
  if (data && Array.isArray(data.results)) return data.results;
  return [];
}

// GET a list endpoint; cb(listOrNull) — null means the request failed.
function apiGetList(path, cb) {
  request('GET', path, null, function (ok, status, data) {
    cb(ok ? asList(data) : null);
  });
}

// POST with optional JSON body; cb(ok, data).
function apiPost(path, body, cb) {
  request('POST', path, body, function (ok, status, data) { cb(ok, data); });
}

// Fetches the "today | overdue" tasks, tolerant of either v1 filter form.
function getTodayTasks(cb) {
  var q = encodeURIComponent('today | overdue');
  request('GET', '/tasks/filter?query=' + q + '&limit=200', null, function (ok, status, data) {
    if (ok) { cb(asList(data)); return; }
    // Fall back to the query-param form if the dedicated filter path isn't there.
    request('GET', '/tasks?filter=' + q + '&limit=200', null, function (ok2, s2, d2) {
      cb(ok2 ? asList(d2) : null);
    });
  });
}

// --- AppMessage senders ------------------------------------------------------

function sendLoad(state) {
  var msg = {};
  msg[keys.LOAD_STATE] = state;
  Pebble.sendAppMessage(msg);
}

// Streams the project list one message at a time, advancing on each ACK.
function streamProjects(projects) {
  var count = Math.min(projects.length, MAX_PROJECTS);
  var head = {};
  head[keys.PROJ_COUNT] = count;
  head[keys.LOAD_STATE] = LOAD_OK;
  Pebble.sendAppMessage(head, function () {
    var i = 0;
    function next() {
      if (i >= count) return;
      var p = projects[i];
      var msg = {};
      msg[keys.PROJ_INDEX] = i;
      msg[keys.PROJ_ID] = String(p.id);
      msg[keys.PROJ_NAME] = String(p.name).substring(0, 31);
      msg[keys.PROJ_TASKCOUNT] = p.count || 0;
      Pebble.sendAppMessage(msg, function () { i++; next(); },
        function () { console.log('proj ' + i + ' send failed'); i++; next(); });
    }
    next();
  }, function () { console.log('proj head send failed'); });
}

// Streams the task list one message at a time, advancing on each ACK.
function streamTasks(tasks) {
  var count = Math.min(tasks.length, MAX_TASKS);
  var head = {};
  head[keys.TASK_COUNT] = count;
  head[keys.LOAD_STATE] = LOAD_OK;
  Pebble.sendAppMessage(head, function () {
    var i = 0;
    function next() {
      if (i >= count) return;
      var t = tasks[i];
      var msg = {};
      msg[keys.TASK_INDEX] = i;
      msg[keys.TASK_ID] = String(t.id);
      msg[keys.TASK_TITLE] = String(t.title).substring(0, 127);
      msg[keys.TASK_DUE] = String(t.due || '').substring(0, 31);
      msg[keys.TASK_DONE] = t.done ? 1 : 0;
      Pebble.sendAppMessage(msg, function () { i++; next(); },
        function () { console.log('task ' + i + ' send failed'); i++; next(); });
    }
    next();
  }, function () { console.log('task head send failed'); });
}

var MAX_LABELS = 32;

// Streams the label list one message at a time, advancing on each ACK.
function streamLabels(names) {
  var count = Math.min(names.length, MAX_LABELS);
  var head = {};
  head[keys.LABEL_COUNT] = count;
  head[keys.LOAD_STATE] = LOAD_OK;
  Pebble.sendAppMessage(head, function () {
    var i = 0;
    function next() {
      if (i >= count) return;
      var msg = {};
      msg[keys.LABEL_INDEX] = i;
      msg[keys.LABEL_NAME] = String(names[i]).substring(0, 31);
      Pebble.sendAppMessage(msg, function () { i++; next(); },
        function () { console.log('label ' + i + ' send failed'); i++; next(); });
    }
    next();
  }, function () { console.log('label head send failed'); });
}

// --- Loads -------------------------------------------------------------------

// Fetches a task count for each selected project, then streams the project list.
function loadProjects() {
  var token = getToken();
  var sel = getSel();
  console.log('loadProjects: tokenLen=' + token.length + ' selCount=' + sel.length);
  if (!token || sel.length === 0) { sendLoad(LOAD_UNCONFIGURED); return; }
  sendLoad(LOAD_LOADING);

  var out = [];
  var failures = 0;
  var i = 0;
  function step() {
    if (i >= sel.length) {
      if (out.length === 0 && failures > 0) { sendLoad(LOAD_ERROR); return; }
      streamProjects(out);
      return;
    }
    var p = sel[i];
    apiGetList('/tasks?project_id=' + encodeURIComponent(p.id) + '&limit=200', function (tasks) {
      if (tasks === null) { failures++; }
      else { taskCache[p.id] = tasks; }
      out.push({ id: p.id, name: p.name, count: tasks ? tasks.length : 0 });
      i++;
      step();
    });
  }
  step();
}

// Short month names per UI language. ASCII-only except the é that Pebble's GOTHIC
// font is known to render; avoids the pkjs runtime's unreliable Intl support.
var DUE_MONTHS = {
  en: ['Jan','Feb','Mar','Apr','May','Jun','Jul','Aug','Sep','Oct','Nov','Dec'],
  nl: ['jan','feb','mrt','apr','mei','jun','jul','aug','sep','okt','nov','dec'],
  fr: ['janv','févr','mars','avr','mai','juin','juil','aout','sept','oct','nov','déc'],
  de: ['Jan','Feb','Mrz','Apr','Mai','Jun','Jul','Aug','Sep','Okt','Nov','Dez'],
  es: ['ene','feb','mar','abr','may','jun','jul','ago','sep','oct','nov','dic']
};
function localeFor(langIdx) { return ['en','nl','fr','de','es'][langIdx] || 'en'; }

// Compact human-readable due label from a Todoist task's `due` object; '' if none.
function formatDue(due) {
  if (!due) { return ''; }
  var mon = DUE_MONTHS[localeFor(getPageLang())] || DUE_MONTHS.en;
  function dm(d, mIdx) { return d + ' ' + (mon[mIdx] || ''); }
  if (due.datetime) {
    var dt = new Date(due.datetime);
    if (!isNaN(dt.getTime())) {
      var hh = ('0' + dt.getHours()).slice(-2), mm = ('0' + dt.getMinutes()).slice(-2);
      return dm(dt.getDate(), dt.getMonth()) + ' ' + hh + ':' + mm;
    }
  }
  if (due.date) {
    var p = String(due.date).split('-');
    if (p.length === 3) { return dm(parseInt(p[2], 10), parseInt(p[1], 10) - 1); }
  }
  return String(due.string || '');
}

// Maps Todoist task objects to the small {id,title,done,due} the watch needs.
// `prefixProject` adds "Project · " for the mixed Vandaag view.
function mapTasks(raw, prefixProject) {
  return (raw || []).map(function (t) {
    var title = String(t.content || '');
    if (prefixProject) {
      var nm = selNameById(String(t.project_id));
      if (nm) title = nm + ' · ' + title;
    }
    var done = t.is_completed || t.checked || t.completed || false;
    return { id: t.id, title: title, done: !!done, due: formatDue(t.due) };
  });
}

var LABEL_PREFIX = 'label:';

// Fetches the tasks carrying a given label, tolerant of either v1 filter form.
// Note: Todoist's `@name` filter matches single-token label names; a label name
// with spaces may not match (consistent with the app's pragmatic scope).
function getLabelTasks(name, cb) {
  var q = encodeURIComponent('@' + name);
  request('GET', '/tasks/filter?query=' + q + '&limit=200', null, function (ok, status, data) {
    if (ok) { cb(asList(data)); return; }
    request('GET', '/tasks?filter=' + q + '&limit=200', null, function (ok2, s2, d2) {
      cb(ok2 ? asList(d2) : null);
    });
  });
}

function loadTasks(projectId) {
  var token = getToken();
  if (!token) { sendLoad(LOAD_UNCONFIGURED); return; }
  current.projectId = projectId;
  sendLoad(LOAD_LOADING);

  var isToday = (projectId === TODAY_ID);
  var isLabel = projectId.indexOf(LABEL_PREFIX) === 0;
  var mixed = isToday || isLabel;   // prefix rows with the project name (cross-project view)
  function done(raw) {
    if (raw === null) { sendLoad(LOAD_ERROR); return; }
    if (!mixed) { taskCache[projectId] = raw; }
    streamTasks(mapTasks(raw, mixed));
  }
  if (isToday) {
    getTodayTasks(done);
  } else if (isLabel) {
    getLabelTasks(projectId.slice(LABEL_PREFIX.length), done);
  } else {
    apiGetList('/tasks?project_id=' + encodeURIComponent(projectId) + '&limit=200', done);
  }
}

// Fetches the user's labels and streams their names to the watch.
function loadLabels() {
  var token = getToken();
  if (!token) { sendLoad(LOAD_UNCONFIGURED); return; }
  sendLoad(LOAD_LOADING);
  apiGetList('/labels?limit=200', function (labels) {
    if (labels === null) { sendLoad(LOAD_ERROR); return; }
    streamLabels(labels.map(function (l) { return l.name; }));
  });
}

// Fetches one task's description + labels on demand for the detail view. Always
// replies (empty on failure) so the watch stops waiting.
function sendTaskDetail(taskId) {
  if (!taskId) { return; }
  request('GET', '/tasks/' + encodeURIComponent(taskId), null, function (ok, status, data) {
    var msg = {};
    msg[keys.TASK_DETAIL_ID] = String(taskId);
    if (ok && data) {
      msg[keys.TASK_DETAIL_DESC] = String(data.description || '').substring(0, 500);
      msg[keys.TASK_DETAIL_LABELS] = (data.labels || []).join(',').substring(0, 150);
    } else {
      msg[keys.TASK_DETAIL_DESC] = '';
      msg[keys.TASK_DETAIL_LABELS] = '';
    }
    Pebble.sendAppMessage(msg);
  });
}

// Splits a leading or trailing Dutch date phrase off the spoken text so it can be
// passed as `due_string` on its own (Todoist can't parse a date out of a whole
// sentence). Todoist still interprets the phrase; we only separate it.
// e.g. "woensdag de nieuwe klassen aanpassen" -> {content:"de nieuwe klassen aanpassen", due:"woensdag"}
function splitDueDate(text) {
  var t = String(text).trim();
  var WD = 'maandag|dinsdag|woensdag|donderdag|vrijdag|zaterdag|zondag';
  var REL = 'vandaag|morgen|overmorgen|vanavond|vanmiddag|vanochtend|vannacht';
  var MOD = 'aanstaande|komende|volgende|deze';
  var core = '(?:over\\s+\\d+\\s+(?:dag(?:en)?|we(?:ek|ken)|maand(?:en)?)' +
             '|(?:' + MOD + ')\\s+we(?:ek|ken)(?:\\s+(?:' + WD + '))?' +
             '|(?:' + MOD + ')\\s+maand(?:en)?' +
             '|(?:' + MOD + ')\\s+(?:' + WD + ')' +
             '|(?:' + WD + ')' +
             '|(?:' + REL + '))';

  // Leading: optional "op ", the date phrase, then the task text.
  var lead = new RegExp('^(?:op\\s+)?(' + core + ')\\b[\\s,]*', 'i');
  var m = t.match(lead);
  if (m) {
    var rest = t.slice(m[0].length).trim();
    if (rest) { return { content: rest, due: m[1] }; }
  }
  // Trailing: the task text, then optional "op " and the date phrase.
  var trail = new RegExp('[\\s,]*(?:op\\s+)?(' + core + ')\\s*$', 'i');
  m = t.match(trail);
  if (m) {
    var head = t.slice(0, m.index).trim();
    if (head) { return { content: head, due: m[1] }; }
  }
  return { content: t, due: '' };
}

// Todoist's Dutch date parser accepts "volgende" but rejects "komende"/"aanstaande"
// and "deze <dag>" (HTTP 400). Rewrite those to a form it accepts: a bare weekday
// already means the coming one; "deze week/maand" -> "volgende week/maand".
function normalizeDue(due) {
  var d = due.toLowerCase().replace(/^op\s+/, '').trim();
  d = d.replace(/\b(komende|aanstaande|deze)\s+(maandag|dinsdag|woensdag|donderdag|vrijdag|zaterdag|zondag)\b/, '$2');
  d = d.replace(/\b(komende|aanstaande|deze)\s+(week|maand)\b/, 'volgende $2');
  return d;
}

function addTask(projectId, content) {
  if (!content) return;
  if (!projectId || projectId === TODAY_ID) { projectId = getDefId(); }
  if (!projectId || projectId === TODAY_ID) { sendLoad(LOAD_ERROR); return; }

  var parts = splitDueDate(content);
  var title = parts.content.replace(/\s*\.\s*$/, '');  // drop a dictated trailing "."
  console.log('addTask: title="' + title + '" due="' + parts.due + '"');

  // Ordered due_string candidates, ending with '' (create without a date). Each
  // failed attempt (400) creates nothing, so retrying can't duplicate the task.
  var dues = [];
  if (parts.due) {
    dues.push(parts.due);
    var norm = normalizeDue(parts.due);
    if (norm && dues.indexOf(norm) < 0) { dues.push(norm); }
  }
  dues.push('');  // last resort: plain task

  function attempt(i) {
    var body = { content: title, project_id: projectId };
    if (dues[i]) { body.due_string = dues[i]; body.due_lang = 'nl'; }
    apiPost('/tasks', body, function (ok) {
      if (ok) { loadTasks(projectId); syncTimeline(); return; }
      if (i + 1 < dues.length) { attempt(i + 1); }
      else { sendLoad(LOAD_ERROR); }
    });
  }
  attempt(0);
}

function closeTask(taskId) {
  if (!taskId) return;
  apiPost('/tasks/' + encodeURIComponent(taskId) + '/close', null, function (ok) {
    if (ok) { loadTasks(current.projectId); syncTimeline(); }
    else { sendLoad(LOAD_ERROR); }
  });
}

function deleteTask(taskId) {
  if (!taskId) return;
  request('DELETE', '/tasks/' + encodeURIComponent(taskId), null, function (ok) {
    if (ok) { loadTasks(current.projectId); syncTimeline(); }
    else { sendLoad(LOAD_ERROR); }
  });
}

// --- Timeline pins -----------------------------------------------------------
// Pushes upcoming tasks (a short window ahead, from the selected projects only)
// to the user's Pebble timeline via Rebble's web API, keyed by task id. Proven
// to work on a sideloaded app: getTimelineToken returns a token and Rebble's
// timeline-api.rebble.io accepts pins pushed with it. Entirely phone-side; the
// watch C code is not involved. Opt-in via the TIMELINE_ENABLED setting.
var TIMELINE_API = 'https://timeline-api.rebble.io/v1/user/pins/';
var TL_WINDOW_DAYS = 3;   // pin tasks due today .. +3 days
var TL_ALLDAY_HOUR = 8;   // all-day tasks (no due time) get pinned at 08:00 local
var TL_MAX_PINS = 30;
var TL_ICON = 'system://images/TIMELINE_CALENDAR';
var TL_COLOR = '#E44332';  // Todoist red
var s_tlToken = null;      // per-session cache of the timeline token

function getTimelineEnabled() { return localStorage.getItem('TL_ENABLED') === '1'; }
// Map of { pinId: signature } for the pins we currently have on the timeline,
// so a sync can diff and only PUT changed pins / DELETE removed ones.
function getPinMap() { try { return JSON.parse(localStorage.getItem('TL_PINS') || '{}'); } catch (e) { return {}; } }
function setPinMap(m) { localStorage.setItem('TL_PINS', JSON.stringify(m)); }

// Runs cb(token) with a timeline token (cached for the session); cb(null) on failure.
function withTimelineToken(cb) {
  if (s_tlToken) { cb(s_tlToken); return; }
  Pebble.getTimelineToken(
    function (t) { s_tlToken = t; cb(t || null); },
    function (e) { console.log('timeline token failed: ' + JSON.stringify(e)); cb(null); });
}

// PUT/DELETE a pin on the Rebble timeline; cb(ok, status).
function tlRequest(method, id, body, cb) {
  withTimelineToken(function (token) {
    if (!token) { cb(false, 0); return; }
    var xhr = new XMLHttpRequest();
    xhr.open(method, TIMELINE_API + id);
    xhr.setRequestHeader('X-User-Token', token);
    if (body) { xhr.setRequestHeader('Content-Type', 'application/json'); }
    xhr.timeout = 15000;
    xhr.onload = function () { cb(xhr.status >= 200 && xhr.status < 300, xhr.status); };
    xhr.onerror = function () { cb(false, 0); };
    xhr.ontimeout = function () { cb(false, 0); };
    xhr.send(body ? JSON.stringify(body) : null);
  });
}

// A task's pin time as a Date: its due datetime, or all-day due at TL_ALLDAY_HOUR
// local. Null if the task has no usable due date.
function taskPinTime(t) {
  var due = t.due;
  if (!due) { return null; }
  if (due.datetime) { var d = new Date(due.datetime); return isNaN(d.getTime()) ? null : d; }
  if (due.date) {
    var p = String(due.date).split('-');
    if (p.length === 3) {
      var d2 = new Date(parseInt(p[0], 10), parseInt(p[1], 10) - 1, parseInt(p[2], 10),
                        TL_ALLDAY_HOUR, 0, 0);
      return isNaN(d2.getTime()) ? null : d2;
    }
  }
  return null;
}

// Reconciles the timeline with the current upcoming tasks. When the feature is
// off, removes any pins we previously pushed. Safe to call often.
function syncTimeline() {
  var map = getPinMap();

  if (!getTimelineEnabled()) {
    var stale = Object.keys(map);
    if (!stale.length) { return; }
    stale.forEach(function (id) {
      tlRequest('DELETE', id, null, function (ok) { if (ok) { delete map[id]; setPinMap(map); } });
    });
    return;
  }

  if (!getToken()) { return; }
  var sel = getSel();
  if (!sel.length) { return; }
  var selNames = {};
  sel.forEach(function (p) { selNames[p.id] = p.name; });

  var now = Date.now();
  var dayStart = new Date(); dayStart.setHours(0, 0, 0, 0);
  var windowStart = dayStart.getTime();
  var windowEnd = windowStart + (TL_WINDOW_DAYS + 1) * 86400000;

  // Fetch everything due up to the horizon; filter to the window + selected projects below.
  var q = encodeURIComponent('due before: +' + (TL_WINDOW_DAYS + 1) + ' days');
  request('GET', '/tasks/filter?query=' + q + '&limit=200', null, function (ok, status, data) {
    if (!ok) { console.log('timeline: task fetch failed ' + status); return; }
    var tasks = asList(data);

    var desired = {};  // pinId -> { pin, sig }
    for (var i = 0; i < tasks.length; i++) {
      if (Object.keys(desired).length >= TL_MAX_PINS) { break; }
      var t = tasks[i];
      var pid = String(t.project_id);
      if (!(pid in selNames)) { continue; }
      var when = taskPinTime(t);
      if (!when) { continue; }
      var ms = when.getTime();
      if (ms < windowStart || ms >= windowEnd) { continue; }

      var id = 'pebbledoist-task-' + t.id;
      var title = String(t.content || '(taak)').substring(0, 64);
      var iso = when.toISOString();
      var pin = {
        id: id,
        time: iso,
        layout: {
          type: 'genericPin',
          title: title,
          body: selNames[pid] || '',
          tinyIcon: TL_ICON,
          primaryColor: TL_COLOR
        }
      };
      // Only attach a reminder that still lies in the future (past reminders are
      // pointless and can get the pin rejected).
      if (ms > now) {
        pin.reminders = [
          { time: iso, layout: { type: 'genericReminder', title: title, tinyIcon: TL_ICON } }
        ];
      }
      var sig = iso + '|' + title + '|' + (ms > now ? 'r' : '');
      desired[id] = { pin: pin, sig: sig };
    }

    // Remove pins that are no longer wanted (completed / deleted / rescheduled out).
    Object.keys(map).forEach(function (id) {
      if (!(id in desired)) {
        tlRequest('DELETE', id, null, function (okd) { if (okd) { delete map[id]; setPinMap(map); } });
      }
    });
    // Push new or changed pins.
    Object.keys(desired).forEach(function (id) {
      if (map[id] === desired[id].sig) { return; }  // unchanged
      tlRequest('PUT', id, desired[id].pin, function (okp, st) {
        if (okp) { map[id] = desired[id].sig; setPinMap(map); }
        else { console.log('timeline PUT ' + id + ' -> ' + st); }
      });
    });
    console.log('timeline sync: desired=' + Object.keys(desired).length);
  });
}

// --- Lifecycle ---------------------------------------------------------------

Pebble.addEventListener('ready', function () {
  loadProjects();
  syncTimeline();
});

Pebble.addEventListener('appmessage', function (e) {
  var d = (e && e.payload) || {};
  if (d.REFRESH !== undefined) {
    loadProjects();
  } else if (d.REQUEST_TASKS !== undefined) {
    loadTasks(String(d.PROJECT_ID || ''));
  } else if (d.REQUEST_LABELS !== undefined) {
    loadLabels();
  } else if (d.REQUEST_TASK_DETAIL !== undefined) {
    sendTaskDetail(String(d.REQUEST_TASK_DETAIL));
  } else if (d.ADD_TASK !== undefined) {
    addTask(String(d.PROJECT_ID || ''), String(d.ADD_TASK || ''));
  } else if (d.CLOSE_TASK !== undefined) {
    closeTask(String(d.CLOSE_TASK));
  } else if (d.DELETE_TASK !== undefined) {
    deleteTask(String(d.DELETE_TASK));
  }
});

// Rebuild the settings page with the live project list, then open it.
Pebble.addEventListener('showConfiguration', function () {
  var token = getToken();
  if (!token) {
    clay = new Clay(buildConfig(getPageLang(), null, settingsSnapshot()), null, { autoHandleEvents: false });
    Pebble.openURL(clay.generateUrl());
    return;
  }
  apiGetList('/projects?limit=200', function (projects) {
    if (projects) {
      console.log('showConfig: fetched ' + projects.length + ' projects');
      localStorage.setItem('ALL_PROJECTS', JSON.stringify(projects.map(function (p) {
        return { id: String(p.id), name: String(p.name) };
      })));
    } else {
      console.log('showConfig: GET /projects failed (token invalid or no network?)');
    }
    clay = new Clay(buildConfig(getPageLang(), getAllProjects(), settingsSnapshot()), null,
      { autoHandleEvents: false });
    Pebble.openURL(clay.generateUrl());
  });
});

// Current values used to pre-fill the settings form.
function settingsSnapshot() {
  return {
    token: getToken(),
    sel: getSel(),
    langRaw: getLangRaw(),
    fontSize: getFontSize(),
    quickComplete: getQuickComplete(),
    startView: getStartView(),
    defId: getDefId(),
    tlEnabled: getTimelineEnabled()
  };
}

Pebble.addEventListener('webviewclosed', function (e) {
  if (!e || !e.response) { return; }
  // Read both keyings/conversions; Clay differs by version for array components.
  var sConv = clay.getSettings(e.response);          // convert=true (numeric keys)
  var sRaw = clay.getSettings(e.response, false);    // raw (messageKey-name keys)

  // Clay's raw settings wrap each value as { value: X }; convert=true mangles
  // array components (checkboxgroup) to a scalar, so prefer the raw name-keyed
  // form and unwrap it.
  function unwrap(v) {
    return (v && typeof v === 'object' && !Array.isArray(v) && ('value' in v)) ? v.value : v;
  }
  function pick(name) {
    var c = [sRaw[name], sRaw[keys[name]], sConv[keys[name]]];
    for (var j = 0; j < c.length; j++) {
      if (c[j] !== undefined && c[j] !== null) return unwrap(c[j]);
    }
    return undefined;
  }

  var token = (pick('TODOIST_TOKEN') || '').toString().trim();
  var all = getAllProjects();
  var sel = selectionFromRaw(pick('PROJECTS_SEL'), all);
  var lang = parseInt(pick('LANGUAGE'), 10);
  if (isNaN(lang)) { lang = 255; }  // 255 = Automatic (follow watch locale)
  var fontSize = parseInt(pick('FONT_SIZE'), 10);
  if (!(fontSize >= 0 && fontSize <= 2)) { fontSize = 1; }
  var quickComplete = !!pick('QUICK_COMPLETE');
  var startView = parseInt(pick('START_VIEW'), 10) || 0;
  var defId = (pick('DEFAULT_PROJECT') || '').toString();
  var defName = (defId === TODAY_ID) ? '' : nameForId(defId, all);
  var tlEnabled = !!pick('TIMELINE_ENABLED');

  // Token value is never logged.
  console.log('webviewclosed: tokenLen=' + token.length + ' allProjects=' + all.length +
              ' selected=' + sel.length +
              ' selRawConv=' + JSON.stringify(sConv[keys.PROJECTS_SEL]) +
              ' selRawName=' + JSON.stringify(sRaw.PROJECTS_SEL) +
              ' selKeys=' + Object.keys(sRaw).join(','));

  localStorage.setItem('TOKEN', token);
  localStorage.setItem('SEL', JSON.stringify(sel));
  localStorage.setItem('LANG', String(lang));
  localStorage.setItem('FONT_SIZE', String(fontSize));
  localStorage.setItem('QUICK_COMPLETE', quickComplete ? '1' : '0');
  localStorage.setItem('START_VIEW', String(startView));
  localStorage.setItem('DEF_ID', defId);
  localStorage.setItem('DEF_NAME', defName);
  localStorage.setItem('TL_ENABLED', tlEnabled ? '1' : '0');

  // Language, text size + quick-launch settings go to the watch (persisted there);
  // the token never does.
  var msg = {};
  msg[keys.LANGUAGE] = lang;
  msg[keys.FONT_SIZE] = fontSize;
  msg[keys.QUICK_COMPLETE] = quickComplete ? 1 : 0;
  msg[keys.START_VIEW] = startView;
  msg[keys.DEFAULT_PROJECT_ID] = defId;
  msg[keys.DEFAULT_PROJECT_NAME] = defName;
  Pebble.sendAppMessage(msg, function () { loadProjects(); }, function () { loadProjects(); });
  syncTimeline();  // apply the timeline toggle (push or clear pins)
});

function nameForId(id, all) {
  for (var i = 0; i < all.length; i++) { if (all[i].id === id) return all[i].name; }
  return '';
}

// Turns whatever Clay's checkboxgroup returns into the selected [{id,name}].
// Handles: a parallel boolean array, an array of selected ids/names/indices,
// a comma-separated string, or an object map {index/name: true}.
function selectionFromRaw(raw, all) {
  var sel = [];
  if (raw === undefined || raw === null) return sel;
  // Unwrap a Clay { value: [...] } wrapper if one slipped through.
  if (raw && typeof raw === 'object' && !Array.isArray(raw) && ('value' in raw)) {
    raw = raw.value;
  }
  if (raw === undefined || raw === null) return sel;

  var arr;
  if (typeof raw === 'string') {
    arr = raw.length ? raw.split(',').map(function (x) { return x.trim(); }) : [];
  } else if (Array.isArray(raw)) {
    arr = raw;
  } else if (typeof raw === 'object') {
    arr = [];
    Object.keys(raw).forEach(function (k) { if (raw[k]) arr.push(k); });
  } else {
    return sel;
  }

  var allBool = arr.length === all.length && arr.every(function (v) {
    return v === true || v === false || v === 0 || v === 1;
  });

  for (var i = 0; i < all.length && sel.length < MAX_PROJECTS; i++) {
    var picked;
    if (allBool) {
      picked = !!arr[i];
    } else {
      picked = arr.indexOf(all[i].id) >= 0 || arr.indexOf(all[i].name) >= 0 ||
               arr.indexOf(String(i)) >= 0 || arr.indexOf(i) >= 0;
    }
    if (picked) { sel.push(all[i]); }
  }
  return sel;
}
