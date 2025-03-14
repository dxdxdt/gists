# Action cam footage batch stabilization script
Out in the wild, you can only carry too many Gopro batteries and you can never
be sure when you can charge them. You can extend battery life by turning off
stabilization, which is quite battery intensive. But the problem is that you
have to post process the footage when you get back to the civilization. Footage
taken without stabilization on is generally unusable because it's too "shaky".

The post process can take a very long time depending on the amount of footage
you took. Gyroflow is a great tool, but it was not made by Unix programmers so
it has poor CUI and scripting support.

Actions cams like Gopro produce what's called "low resolution video"(LRV) files
in addition to the actual footage on the fly by default. These LRV files are for
on-device playback and "proxy clips" in video editor software. LRV files need to
be regenerated as well for good editing experience.

It seemed obvious that I messed up and had to come up with a solution. At first,
Makefile seemed like a good straightforward answer, but the way Gyroflow was
designed made it impossible to incorporate it. So naturally, I ended up writing
the Python script.

## The script
The script had to be reentrant because the VAAPI support on Linux isn't quite
there yet. The driver can crash the window manager at any given moment, the
external USB hard drive can pop out any time due to insufficient power and so
on. The script had to be resilient to hardware failures.

Gyroflow and FFmpeg outputs files suffixed with ".tmp.mp4". After a successful
run, the ".tmp" suffix is removed. Upon reentry, they're simply overwritten.

On startup, the script looks for all .mp4 files that are **NOT** suffixed with
".tmp" or "_stabilized" to skip the files that are already processed from the
previous run.
