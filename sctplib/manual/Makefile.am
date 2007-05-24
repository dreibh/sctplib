EXTRA_DIST = fixunder.sty functions.sty sctp-api-1.0-1.pdf \
             sctp-api-1.0-1.ps sctp-api-1.0-1.tex commands.doc \
             commands.html commands.pdf 

all:

clean:
	rm -f *.toc *.log *.idx *.ind *.aux *~

distclean:
	rm -f *.toc *.log *.idx *.ind *.aux *~ *.dvi

really-clean: clean
	rm -f *.dvi *.ps *.pdf

sctp-api-1.0-1.ps: sctp-api-1.0-1.pdf
	pdf2ps sctp-api-1.0-1.pdf sctp-api-1.0-1.ps

sctp-api-1.0-1.pdf: sctp-api-1.0-1.tex
	pdflatex sctp-api-1.0-1.tex
	pdflatex sctp-api-1.0-1.tex

dis: sctp-api-1.0-1.pdf
	acroread sctp-api-1.0-1.pdf

pdf: sctp-api-1.0-1.pdf

ps: sctp-api-1.0-1.ps
