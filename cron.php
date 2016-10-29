<?php

/**
 * @file
 * Handles incoming requests to fire off regularly-scheduled tasks (cron jobs).
 */

if (!file_exists('core/includes/bootstrap.inc')) {
  if (!empty($_SERVER['DOCUMENT_ROOT']) && file_exists($_SERVER['DOCUMENT_ROOT'] . '/core/includes/bootstrap.inc')) {
    chdir($_SERVER['DOCUMENT_ROOT']);
  }
  elseif (preg_match('@^(.*)[\\\\/]sites[\\\\/][^\\\\/]+[\\\\/]modules[\\\\/]([^\\\\/]+[\\\\/])?elysia(_cron)?$@', getcwd(), $r) && file_exists($r[1] . '/core/includes/bootstrap.inc')) {
    chdir($r[1]);
  }
  else {
    die("Cron Fatal Error: Can't locate bootstrap.inc. Check cron.php position.");
  }
}

/**
 * Root directory of Backdrop installation.
 */
define('BACKDROP_ROOT', getcwd());

include_once BACKDROP_ROOT . '/core/includes/bootstrap.inc';
backdrop_override_server_variables(array('SCRIPT_NAME' => '/core/cron.php'));
backdrop_bootstrap(BACKDROP_BOOTSTRAP_FULL);

if ((variable_get('cron_key') && empty($_GET['cron_key'])) || !empty($_GET['cron_key']) && variable_get('cron_key') != $_GET['cron_key']) {
  watchdog('cron', 'Cron could not run because an invalid key was used.', array(), WATCHDOG_NOTICE);
  backdrop_access_denied();
}
elseif (variable_get('maintenance_mode', 0) && !variable_get('elysia_cron_run_maintenance', FALSE)) {
  watchdog('cron', 'Cron could not run because the site is in maintenance mode.', array(), WATCHDOG_NOTICE);
  backdrop_access_denied();
}
else {
  if (function_exists('elysia_cron_run')) {
    elysia_cron_run();
  }
  else {
    backdrop_cron_run();
  }
}
