# PageHandler Tools

This folder can be ignored for basic usage but contains the source JavaScript code for the main pageHandler.js file in the examples/minimal/data folder.  The npm script here is used to generate that minified file.  It is generated from js-src/src/pageHandler.js by using browserify, babel, and uglify-js to create a compacted file (hard to read, but much smaller and faster than the original human-readble javascript).

To execute the script yourself, you'll need to have node installed on your system (I'm leaving this as an exercise for the reader, as there are many resources online explaining how to do this).  Then, at a command prompt in this (the js-src) folder, run

```
npm install
```

This will download the packages (babel, etc.) needed to minify the javascript.  If the install command is successful, you may then run the following command to regenerate the minified examples/minimal/data/pageHandler.js file from the original js-src/src/pageHandler.js source file:

```
npm run build
```
