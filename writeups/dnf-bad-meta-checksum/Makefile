# Very very dangerous. Please run this only in a container
REPO_CONF = /etc/yum.repos.d/test-bad-meta-checksum.repo

.PHONY: \
	all \
	test-ok \
	test-failover-404-ok \
	test-failover-checksum-ok \
	clean \
	all \
	install-ok \
	install-faulty \
	uninstall

all: test-ok test-failover-404-ok test-failover-checksum-ok

# sanity test
test-ok: install-ok
	dnf clean all
	dnf makecache --repo=test-bad-meta-checksum
	dnf --disablerepo='*' --enablerepo=test-bad-meta-checksum -C info kernel

# test failover: from 404 to ok
test-failover-404-ok: install-404-ok
	dnf clean all
	dnf makecache --repo=test-bad-meta-checksum
	dnf --disablerepo='*' --enablerepo=test-bad-meta-checksum -C info kernel

# test failover: from bad checksum to ok
test-failover-checksum-ok: install-checksum-ok
	dnf clean all
	dnf makecache --repo=test-bad-meta-checksum
	dnf --disablerepo='*' --enablerepo=test-bad-meta-checksum -C info kernel

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
# Only one good repomd for sanity check
test-metalink.ok.xml: craft-metalink.sh ok
	./craft-metalink.sh repomd.xml ok/repodata/repomd.xml \
			> test-metalink.ok.xml.tmp
	mv -f test-metalink.ok.xml.tmp test-metalink.ok.xml
# Bad, and then good. In that order!
test-metalink.404-ok.xml: craft-metalink.sh ok 404
	./craft-metalink.sh repomd.xml 404/repodata/repomd.xml ok/repodata/repomd.xml \
			> test-metalink.404-ok.xml.tmp
	mv -f test-metalink.404-ok.xml.tmp test-metalink.404-ok.xml
test-metalink.checksum-ok.xml: craft-metalink.sh ok checksum
	./craft-metalink.sh repomd.xml checksum/repodata/repomd.xml ok/repodata/repomd.xml \
			> test-metalink.checksum-ok.xml.tmp
	mv -f test-metalink.checksum-ok.xml.tmp test-metalink.checksum-ok.xml

# Craft repos
install-ok: ./craft-repo.sh test-metalink.ok.xml
	./craft-repo.sh test-metalink.ok.xml > repo.tmp
	mv -f repo.tmp $(REPO_CONF)
install-404-ok: ./craft-repo.sh test-metalink.404-ok.xml
	./craft-repo.sh test-metalink.404-ok.xml > repo.tmp
	mv -f repo.tmp $(REPO_CONF)
install-checksum-ok: ./craft-repo.sh test-metalink.checksum-ok.xml
	./craft-repo.sh test-metalink.checksum-ok.xml > repo.tmp
	mv -f repo.tmp $(REPO_CONF)

uninstall:
	rm -f $(REPO_CONF)
