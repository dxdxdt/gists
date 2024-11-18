BASE_URL ?= http://git.kernel.org/cgit-data
OBJ = \
	cgit.css \
	source-code-pro/source-code-pro.css \
	source-code-pro/WOFF2/TTF/SourceCodePro-Regular.ttf.woff2 \
	source-code-pro/WOFF2/TTF/SourceCodePro-Bold.ttf.woff2 \
	source-code-pro/OTF/SourceCodePro-Regular.otf.woff \
	source-code-pro/OTF/SourceCodePro-Bold.otf.woff \
	source-code-pro/OTF/SourceCodePro-Regular.otf \
	source-code-pro/OTF/SourceCodePro-Bold.otf \
	source-code-pro/TTF/SourceCodePro-Regular.ttf \
	source-code-pro/TTF/SourceCodePro-Bold.ttf

all: ${OBJ}

clean:
	rm -f cgit.css
	rm -rf source-code-pro

${OBJ}:
	mkdir -p $(dir $@)
	curl -sSL ${BASE_URL}/$@ -o $@
