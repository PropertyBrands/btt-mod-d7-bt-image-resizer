# Resize image file uploads

Sometimes, users forget to resize images before they upload them to their website. 

Sometimes, developers forget to set a maximum upload size on fields.

Then, our servers get full. 

## Solution

This module provides a `drush` command to cycle back through previously uploaded images
and resize them to a "reasonable" width or height.

@todo: Provide a way to do this at the time of the upload.

## Usage

To execute a batch process use:

`drush resize-images --width=2000|--height=100 --mime=jpeg|png`
