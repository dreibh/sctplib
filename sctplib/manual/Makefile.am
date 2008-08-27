EXTRA_DIST = fixunder.sty functions.sty sctp-api-1.0.8.pdf \
             sctp-api-1.0.8.ps sctp-api-1.0.8.tex commands.doc \
             commands.html commands.pdf 

all:

clean:
	rm -f *.toc *.log *.idx *.ind *.aux *~

distclean:
	rm -f *.toc *.log *.idx *.ind *.aux *~ *.dvi

really-clean: clean
	rm -f *.dvi *.ps *.pdf

sctp-api-1.0.8.ps: sctp-api-1.0.8.pdf
	pdf2ps sctp-api-1.0.8.pdf sctp-api-1.0.8.ps

sctp-api-1.0.8.pdf: sctp-api-1.0.8.tex
	pdflatex sctp-api-1.0.8.tex
	pdflatex sctp-api-1.0.8.tex

dis: sctp-api-1.0.8.pdf
	acroread sctp-api-1.0.8.pdf

pdf: sctp-api-1.0.8.pdf

ps: sctp-api-1.0.8.ps
