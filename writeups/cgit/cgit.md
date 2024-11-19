# My own cgit
I think cgit is neat. No BS like Gitlab CI or Github issues attached. Just pure
static git web interface.

https://wiki.archlinux.org/title/Cgit

I used the Arch Linux wiki guide myself but learned that I'll have to take some
notes for future reference.

## Cache invalidation
Before doing anything, note to yourself: after changing some settings, you have
to delete the cache files yourself if you want to see how they turned out.

```sh
rm -rf /var/cache/cgit
```

Or you could just refresh the page 5 minutes later when cgit recaches
everything.

## Install
```sh
dnf install epel-release
dnf install nginx cgit git fcgiwrap python3-pygments python3-markdown make curl
```

### Prep static content directory root
Don't use the stylesheet that comes with the
package(`/usr/share/cgit/cgit.css`). It doesn't use media queries so it looks
ugly on desktops in dark mode. Let's steal the one from git.kernel.org. First,
prep a directory.

Set up SELinux fcontext beforehand(on SELinux systems):

```sh
semanage fcontext -t httpd_sys_content_t -a '/var/lib/cgit/[^/]+/docroot(/.*)?'
semanage fcontext -t httpd_sys_script_exec_t -a '/var/lib/cgit/[^/]+/bin(/.*)?'
```

Prep the directories.

```sh
mkdir -p \
	/var/lib/cgit/site \
	/var/lib/cgit/site/docroot \
	/var/lib/cgit/site/bin
```

Download the stylesheet and the fonts from git.kernel.org. I made [the
makefile](Makefile) you can place in the directory and run it.

```sh
cd /var/lib/cgit/site/docroot
curl -sSLO https://raw.githubusercontent.com/dxdxdt/gists/refs/heads/master/writeups/cgit/Makefile
make -j 2
```

For the logo, you have two choices: bring your own or use the one that comes
with the package. For the latter, you can just copy or hardlink.

```sh
ln /usr/share/cgit/cgit.png
```

### Syntax highlighting and README formatter
The use of formatters are optional, but cgit wouldn't be whole without them. The
only good formatters are Python implementations. Ironic if you think about it
because bringing Python to the mix defeats the purpose of using cgit over other
bloated Git web interface. But the good thing is cgit caches everything heavily,
so the Python scripts won't on every single GET request.

The formatters are not so configurable. The code style parameter is hardcoded.
If you wish to use the code style other than the default one, you have to change
the scripts themselves.

First, see what styles are available on your system.

```
$ python3
>>> from pygments.styles import get_all_styles
>>> list(get_all_styles())
['manni', 'igor', 'xcode', 'vim', 'autumn', 'vs', 'rrt', 'native', 'perldoc',
'borland', 'tango', 'emacs', 'friendly', 'monokai', 'paraiso-dark', 'colorful',
'murphy', 'bw', 'pastie', 'paraiso-light', 'trac', 'default', 'fruity']
```

And using the following page for reference, pick the one you desire. You'll have
to do some trial-and-error because some styles don't work well with dark mode. I
recommend **monokai**.

https://pygments.org/styles/

Copy the formatters to the directory.

```sh
cd /var/lib/cgit/site/bin

cp -a /usr/libexec/cgit/filters .
# or
cp -a /usr/lib/cgit/filters .
```

In `filters/syntax-highlighting.py` and `filters/html-converters/md2html`,
change the parameter to **HtmlFormatter** class to change the style.

```py
HtmlFormatter(style='STYLE', nobackground=True)
```

Additionally, in `filters/html-converters/md2html`, remove text color from the
CSS output. That will make text look better in dark mode.

```sh
sed -E -e '/^(\s)+color:/d' -e '/^(\s)+background-color:(\s+)#/d' filters/html-converters/md2html > md2html.tmp &&
	mv md2html.tmp filters/html-converters/md2html
```

Here's the patch for reference.

```patch
23d22
<     color: #c00;
45d43
<     color: #000;
58d55
<     color: black;
65d61
<     color: #000;
70d65
<     color: #000;
82d76
<     color: #777;
91d84
<     color: #ccc;
157d149
<     color: #777;
174d165
<     background-color: #fff;
177d167
<     background-color: #f8f8f8;
203d192
<     color: #333;
262d250
<     background-color: #f8f8f8;
276d263
<     background-color: #f8f8f8;
291c277
< sys.stdout.write(HtmlFormatter(style='pastie').get_style_defs('.highlight'))
---
> sys.stdout.write(HtmlFormatter(style='monokai', nobackground=True).get_style_defs('.highlight'))
```

### /etc/cgitrc
If you've followed this manual thus far, you want to use these instead of
the defaults in `/etc/cgitrc`.

```
source-filter=/var/lib/cgit/site/bin/filters/syntax-highlighting.py
about-filter=/var/lib/cgit/site/bin/filters/about-formatting.sh
```

For the rest, refer to [the arch wiki page](https://wiki.archlinux.org/title/Cgit).

### Hosting on sub URL, not subdomain (RHEL)
I'm not a big fan of Let's Encrypt so all my sites are TLS'd with traditional
certificates. The problem is that I'm not using a wildcard cert so I'd need
another cert for cgit. I didn't want to do that, so I had to come up with the
way to host cgit at `/cgit`.

First, add this to `/etc/cgitrc`.

```
virtual-root=/cgit/
```

And here's the `nginx.conf` I came up with.

```
location /cgit-data {
        alias /var/lib/cgit/site/docroot;
}

location /cgit {
        include                 mime.types;
        default_type            application/octet-stream;
        sendfile                on;
        keepalive_timeout       65;
        gzip                    on;

        # not required for subdomain
        if ( $uri ~* ^/cgit(/.*)?$ ) {
                set $uri_new $1;
        }

        include             fastcgi_params;
        fastcgi_param       SCRIPT_FILENAME /var/www/cgi-bin/cgit;
        # change to $uri if subdomain
        fastcgi_param       PATH_INFO       $uri_new;
        fastcgi_param       QUERY_STRING    $args;
        fastcgi_param       HTTP_HOST       $server_name;
        fastcgi_pass        unix:/run/fcgiwrap/fcgiwrap-cgit.sock;

        location ~ /cgit/.+/(info/refs|git-upload-pack) {
                # not required for subdomain
                if ( $uri ~* ^/cgit(/.*)?$ ) {
                        set $uri_new $1;
                }
                include             fastcgi_params;
                fastcgi_param       SCRIPT_FILENAME     /usr/libexec/git-core/git-http-backend;
                # change to $uri if subdomain
                fastcgi_param       PATH_INFO           $uri_new;
                fastcgi_param       GIT_HTTP_EXPORT_ALL 1;
                fastcgi_param       GIT_PROJECT_ROOT    /srv/git;
                fastcgi_param       HOME                /srv/git;
                fastcgi_pass        unix:/run/fcgiwrap/fcgiwrap-cgit.sock;
        }
}
```

Setting up a system user for cgit.

```sh
useradd -r cgit
usermod -aG cgit nginx

chown cgit /var/cache/cgit

systemctl enable --now fcgiwrap@cgit.socket
systemctl restart nginx.service
```

Disable ownership check. Otherwise, you'll get `fatal: detected dubious
ownership in repository at ...`.

```sh
cat << EOF > /srv/git/.gitconfig
[safe]
	directory = *
EOF
chown cgit:cgit /srv/git/.gitconfig
```

### Mime types
Image types in about pages:

<pre>
mimetype.gif=image/gif
mimetype.html=text/html
mimetype.jpg=image/jpeg
mimetype.jpeg=image/jpeg
mimetype.pdf=application/pdf
mimetype.png=image/png
mimetype.svg=image/svg+xml
<b>mimetype.webp=image/webp</b>
</pre>

### Publish repos
You've got two options:

 1. Use `scan-path=` in `/etc/cgitrc`
 2. Handcraft a list of repos to publish using `repo.*=`

That's it. Enjoy!
