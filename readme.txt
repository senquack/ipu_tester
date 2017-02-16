This utility was created to help verify certain problemlatic resolutions
on the GCW Zero handheld are scaled properly.

Run 'ipu_tester -h' to get a list of command-line options.

All resolutions seem to work fine when screen depth is 32.

However, when screen depth is 16, only resolutions with width that
is multiple of 16 are free from problems. If the width is not a multiple
of 16, there is a high chance that the screen will go black and system
then reboots. Some resolutions that are not multiples of 16, such as
360, 364, 372, do work, so the pattern is hard to ascertain.

All heights 1..480 seem to work regardless of screen depth.

Using stock firmware Aug 20, 2014, a width of 410 will cause screen to
fade to white before rebooting, a unique problem. NOTE: as mentioned
above, this only occurs when screen depth is 16.
