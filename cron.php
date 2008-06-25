<?php
// $Id$

/**
 * @file
 * Handles incoming requests to fire off regularly-scheduled tasks (cron jobs).
 */

include_once './includes/bootstrap.inc';
drupal_bootstrap(DRUPAL_BOOTSTRAP_FULL);

if (function_exists('elysia_cron_run'))
  elysia_cron_run();
else
  drupal_cron_run();