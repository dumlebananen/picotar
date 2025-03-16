I couldn't find a version of tar that could write to tape drives on windows, or if they we're supposed to work, I couldn't get them to work.
So i've used and modified this library https://github.com/rxi/microtar
The modifications is I'm using WIN32 api to create/read/write files instead of open/read/write etc.
Reason for this is that the only info I could find on accessing tape drives with c on windows was through the win32 api.

This code is neither good or complete, but writing multiple files to a tar archive on tape works.

Next thing to implement is command line arguments and recursion
