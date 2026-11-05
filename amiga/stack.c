/* The Puzzle Pits keeps its pits in globals and recurses only in its
 * path finding, but a Shell's 4 KB is too little for its menus' locals
 * and the C library's stdio; libnix's startup sets this much up instead.
 * The AmiCradle Team. Apache 2.0, like The Puzzle Pits. */
unsigned long __stack = 128 * 1024;
