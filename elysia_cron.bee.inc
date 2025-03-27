<?php
/**
 * @file
 * Bee integration for Elysia cron module.
 */

/**
 * Detect: is it bee or not?
 *
 * @return bool
 *   TRUE if code executed inside bee, FALSE otherwise.
 */
function elysia_cron_bee_detect() {
  return (empty($_SERVER['SERVER_SOFTWARE']) && (php_sapi_name() == 'cli' || (is_numeric($_SERVER['argc']) && $_SERVER['argc'] > 0))) && function_exists('bee_error_handler');
}

/**
 * Wrapper for bee_invoke().
 */
function elysia_cron_bee_invoke() {
  bee_parse_input();
  global $_bee_arguments, $_bee_options;
  $args = $_bee_arguments;
  $options = $_bee_options;
  array_shift($args);

  // If bee command has no arguments or the first argument is not in the
  // list of allowed operations then we assume the cron execution.
  if (empty($args) || !in_array($args[0], array('list', 'run', 'enable', 'disable'))) {
    $args = array('op' => 'run');
  }
  else {
    $args['op'] = $args[0];
  }

  call_user_func_array('bee_elysia_cron_run_wrapper', array($args, $options));
  backdrop_exit();
}

/**
 * Implements hook_bee_command().
 */
function elysia_cron_bee_command() {
  return array(
    'elysia-cron' => array(
      'description' => bt('Run all cron tasks in all active modules for specified site using elysia cron system. This replaces the standard "cron" bee handler.'),
      'callback' => 'bee_elysia_cron_run_wrapper',
      'command_requirements' => array(
        'backdrop_root' => FALSE,
        'backdrop_installed' => FALSE,
      ),
      'group' => 'Elysia cron',
      'arguments' => array(
        'op' => 'Operation: list, run, disable, enable',
        'target' => 'Target of operation (optional): usually a task name or a channel name starting with "@"',
      ),
      'optional_arguments' => array('target'),
      'options' => array(
        'quiet' => array(
          'description' => bt('Suppress all output.'),
        ),
        'verbose' => array(
          'description' => bt('Enable extended output.'),
        ),
        'elysia-cron-verbose' => array(
          'description' => bt('Enable extended output (the same as --verbose, but without enabling bee verbose mode).'),
        ),
        'force' => array(
          'description' => bt('Run tasks even if disabled/not ready for execution/already running.'),
        ),
        'ignore-disable' => array(
          'description' => bt('Run tasks even if disabled.'),
        ),
        'ignore-maintenance' => array(
          'description' => bt('Run tasks even if maintenance mode is enabled.'),
        ),
        'ignore-time' => array(
          'description' => bt('Run tasks even if not ready for execution.'),
        ),
        'ignore-running' => array(
          'description' => bt('Run tasks even if already running.'),
        ),
      ),
      'bootstrap' => BEE_BOOTSTRAP_FULL,
      'examples' => array(
        'elysia-cron run' => 'Run all cron tasks in all active modules (as the standard "core-cron")',
        'elysia-cron run --verbose' => 'Run all cron tasks in verbose mode',
        'elysia-cron run @channel' => 'Run all cron tasks in specified channel',
        'elysia-cron run search_cron --ignore-time' => 'Run only search index build task (force execution)',
        'elysia-cron list --elysia-cron-verbose' => 'List all tasks in verbose mode',
        'elysia-cron disable search_cron' => 'Disable search index build task',
      ),
    ),
  );
}

/**
 * Custom callback for 'elysia-cron' bee command.
 */
function bee_elysia_cron_run_wrapper($arguments, $options) {
  global $_elysia_cron_bee;

  // Make sure all the options have values before testing.
  $options += array(
    'quiet' => FALSE,
    'verbose' => FALSE,
    'elysia-cron-verbose' => FALSE,
    'force' => FALSE,
    'ignore-disable' => FALSE,
    'ignore-maintenance' => FALSE,
    'ignore-time' => FALSE,
    'ignore-running' => FALSE,
  );

  if (config_get('system.core', 'maintenance_mode')) {
    $ignore_maintenance = $options['force'] || $options['ignore-maintenance'];
    if (!$ignore_maintenance && config_get('elysia_cron', 'run_maintenance') != TRUE) {
      bee_message(bt('Cron run is not allowed in maintenance mode'), 'error');
      return;
    }
  }

  if (!$options['verbose']) {
    $options['verbose'] = $options['elysia-cron-verbose'];
  }

  if ($options['quiet']) {
    $_elysia_cron_bee = 1;
  }
  elseif ($options['verbose']) {
    $_elysia_cron_bee = 3;
  }
  else {
    $_elysia_cron_bee = 2;
  }

  switch ($arguments['op']) {
    case 'list':
      global $_elysia_cron_settings_by_channel;
      elysia_cron_initialize();

      foreach ($_elysia_cron_settings_by_channel as $channel => $jobs) {
        $lines = array();
        if (!$options['verbose']) {
          $line = array('@' . $channel);
        }
        else {
          $line = array('Channel: @' . $channel);
          if ($running = elysia_cron_is_channel_running($channel)) {
            $line[] = bt('Running, since !time', array('!time' => elysia_cron_date($running)));
          }
          if (!empty($jobs['#data']['disabled'])) {
            $line[] = bt('Disabled');
          }
          if (!$running) {
            $line[] = bt('Last run: !time', array('!time' => elysia_cron_date(state_get('elysia_cron_last_run', 0))));
          }
        }
        bee_message(implode(', ', $line));

        foreach ($jobs as $job => $conf) {
          $line = array();
          if (strpos($job, '#') !== 0) {
            if (!$options['verbose']) {
              bee_message($job);
            }
            else {
              $line['name'] = $job;
              $line['status'] = empty($conf['disabled']) ? bt('Enabled') : bt('Disabled');
              $line['run_status'] = empty($conf['running']) && elysia_cron_should_run($conf) ? bt('Ready to run') : bt('Waiting');
              $line['run_status'] = !empty($conf['running']) ? bt('Running, since !time', array('!time' => elysia_cron_date($conf['running']))) : $line['run_status'];
              $line['last_run'] = !empty($conf['last_run']) ? bt('Last run: !time', array('!time' => elysia_cron_date($conf['last_run']))) : '';
              $lines[] = $line;
            }
          }
        }
        if ($lines) {
          bee_render_table($lines);
        }
      }
      break;

    case 'run':
      // Collect options.
      $ignore_disable = $options['force'] || $options['ignore-disable'];
      $ignore_time = $options['force'] || $options['ignore-time'];
      $ignore_running = $options['force'] || $options['ignore-running'];

      if (empty($arguments['target'])) {
        elysia_cron_run(FALSE, $ignore_disable, $ignore_time, $ignore_running);
      }
      elseif (strpos($arguments['target'], '@') === 0) {
        elysia_cron_initialize();
        if (elysia_cron_channel_exists(substr($arguments['target'], 1))) {
          elysia_cron_run_channel(substr($arguments['target'], 1), $ignore_disable, $ignore_time, $ignore_running);
        }
        else {
          bee_message(bt('Task ' . substr($arguments['target'], 1) . ' does not exist'), 'error');
        }
      }
      else {
        elysia_cron_initialize();
        if (elysia_cron_job_exists($arguments['target'])) {
          elysia_cron_run_job($arguments['target'], $ignore_disable, $ignore_time, $ignore_running);
        }
        else {
          bee_message(bt('Job ' . $arguments['target'] . ' does not exist.'), 'error');
        }
      }
      break;

    case 'disable':
    case 'enable':
      if (!empty($arguments['target'])) {
        if (strpos($arguments['target'], '@') === 0) {
          elysia_cron_set_channel_disabled(substr($arguments['target'], 1), $arguments['op'] == 'disable');
        }
        else {
          elysia_cron_set_job_disabled($arguments['target'], $arguments['op'] == 'disable');
        }
        bee_log('Done', 'ok');
      }
      else {
        bee_message(bt('Target not specified'), 'error');
      }
      break;

    default:
      // Output help?
      help_bee_callback($arguments, $options);
      break;
  }
}
