// Settings-page translations + Clay config builder. Language index matches the
// watch side (src/c/i18n.h): 0=en, 1=nl, 2=fr, 3=de, 4=es.
var LANGS = ['en', 'nl', 'fr', 'de', 'es'];

var L = {
  en: {
    token_desc: 'Enter your Todoist API token (Todoist → Settings → Integrations → Developer). It stays on your phone only and is never sent to the watch.',
    token_label: 'API token', language: 'Language', automatic: 'Automatic (watch language)',
    font_label: 'Text size', font_desc: 'Size of the task titles in a task list.',
    font_small: 'Small', font_medium: 'Medium', font_large: 'Large',
    qc_label: 'Quick-complete', qc_desc: 'Press Select on a task to complete it right away (long-press opens the menu). Off: Select opens the menu.',
    projects_heading: 'Projects on the watch', projects_desc: 'Choose which lists appear on the watch.', projects_label: 'Projects',
    ql_heading: 'On Quick Launch', ql_desc: 'What the app shows when opened via Quick Launch (a normal launch always shows the overview).',
    start_label: 'Start screen', start_overview: 'Project overview', start_dictate: 'Dictate right away', start_project: 'Tasks of default project',
    default_label: 'Default project', today: 'Today', save: 'Save',
    tl_heading: 'Timeline', tl_label: 'Show on timeline',
    tl_desc: 'Show upcoming tasks (next 3 days) as pins on your Pebble timeline, with a reminder at the deadline.',
    notoken: 'Enter your token and save. Then reopen settings to choose your projects.'
  },
  nl: {
    token_desc: 'Vul je Todoist API-token in (Todoist → Instellingen → Integraties → Ontwikkelaar). De token blijft alleen op je telefoon en wordt nooit naar het horloge gestuurd.',
    token_label: 'API-token', language: 'Taal', automatic: 'Automatisch (horlogetaal)',
    font_label: 'Tekstgrootte', font_desc: 'Grootte van de taaktitels in een takenlijst.',
    font_small: 'Klein', font_medium: 'Normaal', font_large: 'Groot',
    qc_label: 'Snel afvinken', qc_desc: 'Druk op Select bij een taak om ze meteen af te vinken (lang indrukken opent het menu). Uit: Select opent het menu.',
    projects_heading: 'Projecten op het horloge', projects_desc: 'Kies welke lijsten op de watch verschijnen.', projects_label: 'Projecten',
    ql_heading: 'Bij Quick Launch', ql_desc: 'Wat de app toont als je ze via Quick Launch opent (een gewone start toont altijd het overzicht).',
    start_label: 'Startscherm', start_overview: 'Projectoverzicht', start_dictate: 'Direct inspreken', start_project: 'Taken van standaardproject',
    default_label: 'Standaardproject', today: 'Vandaag', save: 'Opslaan',
    tl_heading: 'Timeline', tl_label: 'Op timeline tonen',
    tl_desc: 'Toon aankomende taken (komende 3 dagen) als pins op je Pebble-timeline, met een herinnering op de deadline.',
    notoken: 'Vul je token in en sla op. Heropen daarna de instellingen om je projecten te kiezen.'
  },
  fr: {
    token_desc: 'Saisissez votre jeton API Todoist (Todoist → Paramètres → Intégrations → Développeur). Il reste uniquement sur votre téléphone et n\'est jamais envoyé à la montre.',
    token_label: 'Jeton API', language: 'Langue', automatic: 'Automatique (langue de la montre)',
    font_label: 'Taille du texte', font_desc: 'Taille des titres de tâches dans une liste.',
    font_small: 'Petit', font_medium: 'Moyen', font_large: 'Grand',
    qc_label: 'Complétion rapide', qc_desc: 'Appuyez sur Sélection sur une tâche pour la terminer aussitôt (appui long ouvre le menu). Désactivé : Sélection ouvre le menu.',
    projects_heading: 'Projets sur la montre', projects_desc: 'Choisissez les listes à afficher sur la montre.', projects_label: 'Projets',
    ql_heading: 'Au lancement rapide', ql_desc: 'Ce que l\'app affiche quand elle est ouverte via Quick Launch (un lancement normal affiche toujours l\'aperçu).',
    start_label: 'Écran de démarrage', start_overview: 'Aperçu des projets', start_dictate: 'Dicter directement', start_project: 'Tâches du projet par défaut',
    default_label: 'Projet par défaut', today: 'Aujourd\'hui', save: 'Enregistrer',
    tl_heading: 'Timeline', tl_label: 'Afficher sur la timeline',
    tl_desc: 'Affiche les tâches à venir (3 prochains jours) comme des pins sur la timeline Pebble, avec un rappel à l\'échéance.',
    notoken: 'Saisissez votre jeton et enregistrez. Rouvrez ensuite les réglages pour choisir vos projets.'
  },
  de: {
    token_desc: 'Gib deinen Todoist API-Token ein (Todoist → Einstellungen → Integrationen → Entwickler). Er bleibt nur auf deinem Telefon und wird nie an die Uhr gesendet.',
    token_label: 'API-Token', language: 'Sprache', automatic: 'Automatisch (Uhrsprache)',
    font_label: 'Textgröße', font_desc: 'Größe der Aufgabentitel in einer Aufgabenliste.',
    font_small: 'Klein', font_medium: 'Mittel', font_large: 'Groß',
    qc_label: 'Schnell-Erledigen', qc_desc: 'Auf einer Aufgabe Select drücken, um sie sofort zu erledigen (langer Druck öffnet das Menü). Aus: Select öffnet das Menü.',
    projects_heading: 'Projekte auf der Uhr', projects_desc: 'Wähle, welche Listen auf der Uhr erscheinen.', projects_label: 'Projekte',
    ql_heading: 'Bei Quick Launch', ql_desc: 'Was die App beim Öffnen über Quick Launch zeigt (ein normaler Start zeigt immer die Übersicht).',
    start_label: 'Startbildschirm', start_overview: 'Projektübersicht', start_dictate: 'Sofort diktieren', start_project: 'Aufgaben des Standardprojekts',
    default_label: 'Standardprojekt', today: 'Heute', save: 'Speichern',
    tl_heading: 'Timeline', tl_label: 'In Timeline anzeigen',
    tl_desc: 'Anstehende Aufgaben (nächste 3 Tage) als Pins in der Pebble-Timeline anzeigen, mit Erinnerung zum Fälligkeitstermin.',
    notoken: 'Token eingeben und speichern. Öffne danach die Einstellungen erneut, um deine Projekte zu wählen.'
  },
  es: {
    token_desc: 'Introduce tu token de API de Todoist (Todoist → Ajustes → Integraciones → Desarrollador). Permanece solo en tu teléfono y nunca se envía al reloj.',
    token_label: 'Token de API', language: 'Idioma', automatic: 'Automático (idioma del reloj)',
    font_label: 'Tamaño del texto', font_desc: 'Tamaño de los títulos de tareas en una lista.',
    font_small: 'Pequeño', font_medium: 'Mediano', font_large: 'Grande',
    qc_label: 'Completar rápido', qc_desc: 'Pulsa Select en una tarea para completarla al instante (mantén pulsado para el menú). Desactivado: Select abre el menú.',
    projects_heading: 'Proyectos en el reloj', projects_desc: 'Elige qué listas aparecen en el reloj.', projects_label: 'Proyectos',
    ql_heading: 'Con Quick Launch', ql_desc: 'Lo que muestra la app al abrirla con Quick Launch (un inicio normal siempre muestra el resumen).',
    start_label: 'Pantalla de inicio', start_overview: 'Resumen de proyectos', start_dictate: 'Dictar directamente', start_project: 'Tareas del proyecto predeterminado',
    default_label: 'Proyecto predeterminado', today: 'Hoy', save: 'Guardar',
    tl_heading: 'Timeline', tl_label: 'Mostrar en la timeline',
    tl_desc: 'Muestra las tareas próximas (próximos 3 días) como pines en la timeline de Pebble, con un recordatorio en la fecha límite.',
    notoken: 'Introduce tu token y guarda. Luego vuelve a abrir los ajustes para elegir tus proyectos.'
  }
};

function isSelected(id, sel) {
  for (var i = 0; i < (sel || []).length; i++) { if (sel[i].id === id) return true; }
  return false;
}

// langIdx: current UI language; allProjects: [{id,name}] or null; snap: prefill state.
function buildConfig(langIdx, allProjects, snap) {
  snap = snap || {};
  var s = L[LANGS[langIdx] || 'en'];

  var items = [
    { type: 'heading', defaultValue: 'PebbleDoist' },
    { type: 'section', items: [
      { type: 'heading', defaultValue: 'Todoist' },
      { type: 'text', defaultValue: s.token_desc },
      { type: 'input', messageKey: 'TODOIST_TOKEN', label: s.token_label,
        defaultValue: snap.token || '', attributes: { type: 'password', placeholder: '0123abcd…' } }
    ] },
    { type: 'section', items: [
      { type: 'select', messageKey: 'LANGUAGE', label: s.language,
        defaultValue: (snap.langRaw != null && snap.langRaw !== '') ? String(snap.langRaw) : '255',
        options: [
          { label: s.automatic, value: '255' },
          { label: 'English', value: '0' },
          { label: 'Nederlands', value: '1' },
          { label: 'Français', value: '2' },
          { label: 'Deutsch', value: '3' },
          { label: 'Español', value: '4' }
        ] },
      { type: 'text', defaultValue: s.font_desc },
      { type: 'select', messageKey: 'FONT_SIZE', label: s.font_label,
        defaultValue: String(snap.fontSize != null ? snap.fontSize : 1),
        options: [
          { label: s.font_small, value: '0' },
          { label: s.font_medium, value: '1' },
          { label: s.font_large, value: '2' }
        ] },
      { type: 'text', defaultValue: s.qc_desc },
      { type: 'toggle', messageKey: 'QUICK_COMPLETE', label: s.qc_label,
        defaultValue: !!snap.quickComplete }
    ] }
  ];

  if (allProjects && allProjects.length) {
    var names = allProjects.map(function (p) { return p.name; });
    var flags = allProjects.map(function (p) { return isSelected(p.id, snap.sel); });

    items.push({ type: 'section', items: [
      { type: 'heading', defaultValue: s.projects_heading },
      { type: 'text', defaultValue: s.projects_desc },
      { type: 'checkboxgroup', messageKey: 'PROJECTS_SEL', label: s.projects_label,
        options: names, defaultValue: flags }
    ] });

    var defOptions = [{ label: s.today, value: 'today' }];
    allProjects.forEach(function (p) { defOptions.push({ label: p.name, value: String(p.id) }); });

    items.push({ type: 'section', items: [
      { type: 'heading', defaultValue: s.ql_heading },
      { type: 'text', defaultValue: s.ql_desc },
      { type: 'select', messageKey: 'START_VIEW', label: s.start_label,
        defaultValue: String(snap.startView || 0),
        options: [
          { label: s.start_overview, value: '0' },
          { label: s.start_dictate, value: '1' },
          { label: s.start_project, value: '2' }
        ] },
      { type: 'select', messageKey: 'DEFAULT_PROJECT', label: s.default_label,
        defaultValue: snap.defId || 'today', options: defOptions }
    ] });

    items.push({ type: 'section', items: [
      { type: 'heading', defaultValue: s.tl_heading },
      { type: 'text', defaultValue: s.tl_desc },
      { type: 'toggle', messageKey: 'TIMELINE_ENABLED', label: s.tl_label,
        defaultValue: !!snap.tlEnabled }
    ] });
  } else {
    items.push({ type: 'section', items: [
      { type: 'text', defaultValue: s.notoken }
    ] });
  }

  items.push({ type: 'submit', defaultValue: s.save });
  return items;
}

module.exports = { LANGS: LANGS, buildConfig: buildConfig };
