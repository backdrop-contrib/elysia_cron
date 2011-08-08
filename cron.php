<?php

/**
 * @file
 * Handles incoming requests to fire off regularly-scheduled tasks (cron jobs).
 */

if (!file_exists('includes/bootstrap.inc') && preg_match('@^(.*)[\\\\/]sites[\\\\/][^\\\\/]+[\\\\/]modules[\\\\/]([^\\\\/]+[\\\\/])?elysia(_cron)?$@', getcwd(), $r))
  chdir($r[1]);

/**
 * Root directory of Drupal installation.
 */
define('DRUPAL_ROOT', getcwd());

include_once DRUPAL_ROOT . '/includes/bootstrap.inc';
drupal_bootstrap(DRUPAL_BOOTSTRAP_FULL);

global $base_url;
$base_url = str_replace('/'.drupal_get_path('module', 'elysia_cron'), '', $base_url);

if (!isset($_GET['cron_key']) || variable_get('cron_key', 'drupal') != $_GET['cron_key']) {
  watchdog('cron', 'Cron could not run because an invalid key was used.', array(), WATCHDOG_NOTICE);
  drupal_access_denied();
}
elseif (variable_get('maintenance_mode', 0)) {
  watchdog('cron', 'Cron could not run because the site is in maintenance mode.', array(), WATCHDOG_NOTICE);
  drupal_access_denied();
}
else {
  if (function_exists('elysia_cron_run'))
    elysia_cron_run();
  else
    drupal_cron_run();
}
