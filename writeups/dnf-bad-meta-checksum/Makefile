# Very very dangerous. Please run this only in a container
REPO_CONF = /etc/yum.repos.d/test-bad-meta-checksum.repo

.PHONY: \
	all \
	test-ok \
	test-faulty-repomd \
	clean \
	all \
	install-ok \
	install-faulty \
	uninstall

all: test-ok test-faulty-repomd

# sanity test
test-ok: install-ok
	dnf clean all
	dnf makecache --repo=test-bad-meta-checksum
	dnf --disablerepo='*' --enablerepo=test-bad-meta-checksum -C info kernel \
		| tee dnf-info.log

# faulty repomd failover
test-faulty-repomd: install-faulty
	dnf clean all
	dnf makecache --repo=test-bad-meta-checksum
	dnf --disablerepo='*' --enablerepo=test-bad-meta-checksum -C info kernel \
		| tee dnf-info.log

clean:
	rm -rf *.tmp *.log repomd_* repomd.* repodata metalink faulty ok

# Download metalink xml
metalink:
	curl -sSL 'https://mirrors.fedoraproject.org/metalink?repo=fedora-rawhide&arch=x86_64&country=us' \
		> metalink.tmp
	mv -f metalink.tmp metalink

# Extract repomd.xml https url
repomd_url: metalink
	xpath -q -e "string(/metalink/files/file[@name='repomd.xml']/resources/url[@protocol='https'][1])" metalink \
		> repomd_url.tmp
	mv -f repomd_url.tmp repomd_url

# Download repomd.xml
repomd.xml: repomd_url
	curl -sSL `cat repomd_url` > repomd.xml.tmp
	mv -f repomd.xml.tmp repomd.xml

# Populate repodata
repodata: repomd_url repomd.xml
	rm -rf repodata.tmp repodata
	mkdir repodata.tmp
	cp -a repomd.xml repodata.tmp/repomd.xml
	xpath -q -e "/repomd/data/location/@href" repomd.xml | \
		sed -E 's/(\s+)?href="(.*)"(\s+)?/\2/' | \
		./download-repodata.sh repomd_url repodata.tmp
	mv -f repodata.tmp repodata

ok: repodata
	rm -rf ok.tmp ok
	mkdir -p ok.tmp
	cp -a repodata ok.tmp/repodata
	mv -f ok.tmp ok

# Fault injection: delete some files
404: repodata
	rm -rf 404.tmp 404
	mkdir -p 404.tmp
	cp -a repodata 404.tmp/repodata

	rm 404.tmp/repodata/*-primary.*

	mv -f 404.tmp 404

# Fault injection: damage some files
checksum: repodata
	rm -rf checksum.tmp checksum
	mkdir -p checksum.tmp
	cp -a repodata checksum.tmp/repodata

	./inject-fault.sh checksum.tmp/repodata/*-primary.*

	mv -f checksum.tmp checksum

# Craft metalink files
# Only good repomd for sanity check
test-metalink.ok.xml: craft-metalink.sh ok
	./craft-metalink.sh repomd.xml ok/repodata/repomd.xml \
			> test-metalink.ok.xml.tmp
	mv -f test-metalink.ok.xml.tmp test-metalink.ok.xml
# Bad, bad, and then good. In that order!
test-metalink.faulty.xml: craft-metalink.sh ok 404 checksum
	./craft-metalink.sh repomd.xml checksum/repodata/repomd.xml 404/repodata/repomd.xml ok/repodata/repomd.xml \
			> test-metalink.faulty.xml.tmp
	mv -f test-metalink.faulty.xml.tmp test-metalink.faulty.xml

# Craft repos
install-ok: ./craft-repo.sh test-metalink.ok.xml
	./craft-repo.sh test-metalink.ok.xml > repo.tmp
	mv -f repo.tmp $(REPO_CONF)
install-faulty: ./craft-repo.sh test-metalink.faulty.xml
	./craft-repo.sh test-metalink.faulty.xml > repo.tmp
	mv -f repo.tmp $(REPO_CONF)

uninstall:
	rm -f $(REPO_CONF)
