<?php
/**
 * @file - Drush command(s) for bt_image_resized module.
 */

/**
 * Implements hook_drush_command().
 */
function bt_image_resizer_drush_command() {
  $items['resize-images'] = [
    'description' => dt('Resize large images.'),
    'callback' => 'drush_bt_image_resizer_resize',
    'aliases' => ['rsz'],
    'bootstrap' => DRUSH_BOOTSTRAP_DRUPAL_FULL,
    'arguments' => [
      'mime' => dt('MIME Type to process. `jpeg` or `png`.'),
    ],
    'options' => [
      'limit' => [
        'description' => 'Limit the number of images to process.',
      ],
      'dirs' => [
        'description' => 'Comma delimited list of directories to process.',
      ],
      'width' => [
        'description' => 'Resize images by width.',
      ],
      'height' => [
        'description' => 'Resize images by height.',
      ],
    ]
  ];

  return $items;
}

/**
 * `drush` command callback to resize images in a batch process.
 *
 * @param string $mime
 */
function drush_bt_image_resizer_resize($mime = 'jpeg') {

  $width = drush_get_option('width', 4000);
  $height = drush_get_option('height', 0);
  $limit = drush_get_option('limit', 0);
  $directories = drush_get_option('dirs', NULL);

  $batch = [
    'title' => t('Processing resize image Batch'),
    'operations' => [
      ['bt_image_resizer_process_batch', [$mime, $width, $height, $limit, $directories]]
    ],
    'finished' => 'bt_image_resizer_process_batch_finished',
    'init_message' => t('Resize image Batch is starting.'),
    'file' => drupal_get_path('module', 'bt_image_resizer') . '/bt_image_resizer.drush.inc',
  ];

  batch_set($batch);
  $batch =& batch_get();
  // Because we are doing this on the back-end, we set progressive to false.
  $batch['progressive'] = FALSE;
  // Process the batch.
  drush_backend_batch_process();
}

/**
 * Batch processor callback function.
 *
 * Evaluates images that are too large per incoming arguments
 * and re-sizes them. Additionally, `file_metadata` table is updated.
 *
 * @param $mime
 * @param $width
 * @param $height
 * @param $limit
 * @param $directories
 * @param $context
 */
function bt_image_resizer_process_batch($mime, $width, $height, $limit, $directories, &$context) {
  if (empty($context['sandbox'])) {
    // setup progress
    $resize_key = empty($height) ? 'width' : 'height';
    $resize_value = empty($height) ? $width : $height;

    $q = db_select('file_managed', 'fm')
      ->fields('fm', [])
      ->fields('fmtd', ['name', 'value'])
      ->condition('fmtd.name', $resize_key)
      ->condition('filemime', "image/$mime");
    $q->innerJoin('file_metadata', 'fmtd', 'fm.fid=fmtd.fid');

    if($directories) {
      $dirs = explode(',', $directories);
      foreach($dirs as $dir) {
        $q->condition('uri', "$dir%", 'LIKE');
      }
    }

    if($limit) {
      $q->range(0, intval($limit));
    }

    $results = $q->execute()->fetchAll();

    if(empty($results)) {
      drupal_set_message(dt('No images to process'));
      return;
    }

    $items = [];

    foreach($results as $item) {
      $size = unserialize($item->value);
      if(intval($size) > intval($resize_value)) {
        $items[] = $item;
      }
    }

    $context['sandbox']['items'] = $items;
    $context['sandbox']['progress'] = 1;
    $context['sandbox']['index'] = 0;
    $context['sandbox']['max'] = count($items);
    $context['sandbox']['results'] = [];
    $context['sandbox']['resize_type'] = [$resize_key => $resize_value];
  }

  $context['message'] = t('Now processing the files %progress of %max', [
      '%progress' => $context['sandbox']['progress'],
      '%max' => $context['sandbox']['max'],
    ]
  );

  $file = $context['sandbox']['items'][$context['sandbox']['index']];
  $resize_type = $context['sandbox']['resize_type'];

  $image = image_load($file->uri);

  // Scale the image to width/height as requested.
  if(isset($resize_type['width'])) {
    image_scale($image, $resize_type['width']);
  }
  else {
    image_scale($image, NULL, $resize_type['height']);
  }

  // Save the image file invoking hooks.
  image_save($image);

  // Load a proper file entity and rewrite its metadata.
  $file_to_save = file_load($file->fid);
  $file_to_save->metadata = [
    [
      'height' => $image->info['height'],
      'width' => $image->info['width'],
    ],
  ];

  // save
  file_save($file_to_save);

  $context['results'][] = $image;

  // Update batch progress
  $context['sandbox']['progress']++;
  $context['sandbox']['index']++;
  $context['finished'] = $context['sandbox']['progress'] / $context['sandbox']['max'];

}

/**
 * Batch finished callback.
 *
 * @param $success
 * @param $results
 * @param $operations
 */
function bt_image_resizer_process_batch_finished($success, $results, $operations) {
  if($success){
    watchdog('bt_image_resizer', 'Batch success');
    drupal_set_message(t('Success'));
  }
  else{
    watchdog('debug', 'Batch error');
  }
}
