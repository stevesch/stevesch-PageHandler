# PageHandler Tools

This tools folder can be ignored for basic usage.  The npm script here is used to generate the minified pageHandler.js file in the data directory.  It is generated from js-src/pageHandler.js by using browserify, babel, and uglify-js to create a compacted file (hard to read, but much smaller and faster than the original human-readble javascript).

To execute the script yourself, you'll need to have node installed on your system (I'm leaving this as an exercise for the reader, as there are many resources online explaining how to do this).  Then, at a command prompt in the tools folder, run

```
npm install
```

This will download the packages (babel, etc.) needed to minify the javascript.  If the install command is successful, you may then run the following command to regenerate the minified data/pageHandler.js file from the original js-src/pageHandler.js source file:

```
npm run build
```
