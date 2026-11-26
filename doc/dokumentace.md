---
lang: cs
papersize: a4
geometry: margin=2.5cm
fontsize: 11pt
mainfont: TeX Gyre Termes
mathfont: TeX Gyre Termes Math
header-includes:
  - \usepackage{graphicx}
  - \usepackage{array}
  - \usepackage{xcolor}
  - \usepackage{colortbl}
---

\begin{titlepage}
\begin{center}

\vspace*{1cm}
\includegraphics[width=0.77\linewidth]{FIT_barevne_CMYK_CZ.pdf}

\vspace{4cm}

{\LARGE\bfseries Překladač pro jazyk IFJ26\par}
\vspace{0.4cm}
{\large\bfseries Dokumentace k projektu\par}
\vspace{0.4cm}
{\large Varianta: TRP-izp\par}
\vspace{0.4cm}
{\large Git repozitář: \href{https://github.com/sollycz/Kremuler}{\texttt{github.com/sollycz/Kremuler}}\par}

\vspace{2.5cm}

{\large
\begin{tabular}{l l l | l}
Oleksandr Teslja & (xtesljo00) & 282245 & 25 \% \\
David Zoblivý    & (xzoblid00) & 281803 & 25 \% \\
Filip Radulovič  & (xradulf00) & 282100 & 25 \% \\
Tomáš Neckař     & (xneckat00) & 281998 & 25 \% \\
\end{tabular}
\par}

\vspace{2.5cm}

{\large Podporovaná rozšíření:\par}
\vspace{0.2cm}
{\normalsize BOOLTHEN, FUNEXP, CONTROL, INTERPOLATEDS, VALMEM, EXTKW, PARAMLISTS \par}

\vfill

{\large 26. listopadu 2026\par}

\end{center}
\end{titlepage}

\tableofcontents
\newpage

# Práce v týmu

\newpage

# Precedenční tabulka

```{=latex}
\newlength{\cellwidth}
\setlength{\cellwidth}{\dimexpr(\textwidth-21\arrayrulewidth)/20\relax}
\newcolumntype{C}{>{\centering\arraybackslash}m{\cellwidth}}

\begingroup
\setlength{\parindent}{0pt}
\setlength{\tabcolsep}{0pt}
{\small\textbf{Řádek:} nejvrchnější terminál na zásobníku.\par
\textbf{Sloupec:} následující symbol na vstupu.}\par\vspace{4mm}
\begingroup
\fontsize{9}{12}\selectfont
\renewcommand{\arraystretch}{1}
\setlength{\extrarowheight}{3.2mm}
\setlength{\arrayrulewidth}{0.6pt}
\setlength{\cellwidth}{\dimexpr(\textwidth-21\arrayrulewidth)/20\relax}
\begin{tabular}{|*{20}{C|}}
\hline
\rowcolor{gray!18}
 & \texttt{+} & \texttt{-} & \texttt{*} & \texttt{/} & \texttt{<} & \texttt{<=} & \texttt{>} & \texttt{>=} & \texttt{==} & \texttt{!=} & \texttt{u+} & \texttt{u-} & \texttt{!} & \texttt{\&\&} & \texttt{||} & \texttt{(} & \texttt{)} & \texttt{i} & \texttt{\$} \\ \hline
\cellcolor{gray!12}\texttt{+} & $>$ & $>$ & $<$ & $<$ & $>$ & $>$ & $>$ & $>$ & $>$ & $>$ & $<$ & $<$ & $<$ & $>$ & $>$ & $<$ & $>$ & $<$ & $>$ \\ \hline
\cellcolor{gray!12}\texttt{-} & $>$ & $>$ & $<$ & $<$ & $>$ & $>$ & $>$ & $>$ & $>$ & $>$ & $<$ & $<$ & $<$ & $>$ & $>$ & $<$ & $>$ & $<$ & $>$ \\ \hline
\cellcolor{gray!12}\texttt{*} & $>$ & $>$ & $>$ & $>$ & $>$ & $>$ & $>$ & $>$ & $>$ & $>$ & $<$ & $<$ & $<$ & $>$ & $>$ & $<$ & $>$ & $<$ & $>$ \\ \hline
\cellcolor{gray!12}\texttt{/} & $>$ & $>$ & $>$ & $>$ & $>$ & $>$ & $>$ & $>$ & $>$ & $>$ & $<$ & $<$ & $<$ & $>$ & $>$ & $<$ & $>$ & $<$ & $>$ \\ \hline
\cellcolor{gray!12}\texttt{<} & $<$ & $<$ & $<$ & $<$ &  &  &  &  & $>$ & $>$ & $<$ & $<$ & $<$ & $>$ & $>$ & $<$ & $>$ & $<$ & $>$ \\ \hline
\cellcolor{gray!12}\texttt{<=} & $<$ & $<$ & $<$ & $<$ &  &  &  &  & $>$ & $>$ & $<$ & $<$ & $<$ & $>$ & $>$ & $<$ & $>$ & $<$ & $>$ \\ \hline
\cellcolor{gray!12}\texttt{>} & $<$ & $<$ & $<$ & $<$ &  &  &  &  & $>$ & $>$ & $<$ & $<$ & $<$ & $>$ & $>$ & $<$ & $>$ & $<$ & $>$ \\ \hline
\cellcolor{gray!12}\texttt{>=} & $<$ & $<$ & $<$ & $<$ &  &  &  &  & $>$ & $>$ & $<$ & $<$ & $<$ & $>$ & $>$ & $<$ & $>$ & $<$ & $>$ \\ \hline
\cellcolor{gray!12}\texttt{==} & $<$ & $<$ & $<$ & $<$ & $<$ & $<$ & $<$ & $<$ &  &  & $<$ & $<$ & $<$ & $>$ & $>$ & $<$ & $>$ & $<$ & $>$ \\ \hline
\cellcolor{gray!12}\texttt{!=} & $<$ & $<$ & $<$ & $<$ & $<$ & $<$ & $<$ & $<$ &  &  & $<$ & $<$ & $<$ & $>$ & $>$ & $<$ & $>$ & $<$ & $>$ \\ \hline
\cellcolor{gray!12}\texttt{u+} & $>$ & $>$ & $>$ & $>$ & $>$ & $>$ & $>$ & $>$ & $>$ & $>$ & $<$ & $<$ & $<$ & $>$ & $>$ & $<$ & $>$ & $<$ & $>$ \\ \hline
\cellcolor{gray!12}\texttt{u-} & $>$ & $>$ & $>$ & $>$ & $>$ & $>$ & $>$ & $>$ & $>$ & $>$ & $<$ & $<$ & $<$ & $>$ & $>$ & $<$ & $>$ & $<$ & $>$ \\ \hline
\cellcolor{gray!12}\texttt{!} & $>$ & $>$ & $>$ & $>$ & $>$ & $>$ & $>$ & $>$ & $>$ & $>$ & $<$ & $<$ & $<$ & $>$ & $>$ & $<$ & $>$ & $<$ & $>$ \\ \hline
\cellcolor{gray!12}\texttt{\&\&} & $<$ & $<$ & $<$ & $<$ & $<$ & $<$ & $<$ & $<$ & $<$ & $<$ & $<$ & $<$ & $<$ & $>$ & $>$ & $<$ & $>$ & $<$ & $>$ \\ \hline
\cellcolor{gray!12}\texttt{||} & $<$ & $<$ & $<$ & $<$ & $<$ & $<$ & $<$ & $<$ & $<$ & $<$ & $<$ & $<$ & $<$ & $<$ & $>$ & $<$ & $>$ & $<$ & $>$ \\ \hline
\cellcolor{gray!12}\texttt{(} & $<$ & $<$ & $<$ & $<$ & $<$ & $<$ & $<$ & $<$ & $<$ & $<$ & $<$ & $<$ & $<$ & $<$ & $<$ & $<$ & $=$ & $<$ &  \\ \hline
\cellcolor{gray!12}\texttt{)} & $>$ & $>$ & $>$ & $>$ & $>$ & $>$ & $>$ & $>$ & $>$ & $>$ & $>$ & $>$ & $>$ & $>$ & $>$ &  & $>$ &  & $>$ \\ \hline
\cellcolor{gray!12}\texttt{i} & $>$ & $>$ & $>$ & $>$ & $>$ & $>$ & $>$ & $>$ & $>$ & $>$ & $>$ & $>$ & $>$ & $>$ & $>$ &  & $>$ &  & $>$ \\ \hline
\cellcolor{gray!12}\texttt{\$} & $<$ & $<$ & $<$ & $<$ & $<$ & $<$ & $<$ & $<$ & $<$ & $<$ & $<$ & $<$ & $<$ & $<$ & $<$ & $<$ &  & $<$ &  \\ \hline
\end{tabular}
\endgroup
\par\vspace{3mm}
{\small
\(i\) = operand (identifikátor nebo literál);\quad
\texttt{u+}, \texttt{u-} = unární plus a mínus;\quad
\texttt{\$} = hranice výrazu.\par
}
\endgroup
```

\newpage

# Pravidla redukce výrazů

```{=latex}
\begingroup
\setlength{\parindent}{0pt}
\begin{center}
\fontsize{13}{16}\selectfont
\renewcommand{\arraystretch}{1.35}
\begin{tabular}{r@{\hspace{3mm}}l@{\hspace{12mm}}|@{\hspace{12mm}}r@{\hspace{3mm}}l}
1. & \(E \rightarrow E + E\)                          & 10. & \(E \rightarrow E > E\) \\
2. & \(E \rightarrow E - E\)                          & 11. & \(E \rightarrow E\mathbin{\texttt{>=}}E\) \\
3. & \(E \rightarrow E * E\)                          & 12. & \(E \rightarrow E\mathbin{\texttt{==}}E\) \\
4. & \(E \rightarrow E / E\)                          & 13. & \(E \rightarrow E\mathbin{\texttt{!=}}E\) \\
5. & \(E \rightarrow -E\)                             & 14. & \(E \rightarrow E\mathbin{\texttt{\&\&}}E\) \\
6. & \(E \rightarrow +E\)                             & 15. & \(E \rightarrow E\mathbin{\texttt{||}}E\) \\
7. & \(E \rightarrow \mathord{!}E\)                   & 16. & \(E \rightarrow (E)\) \\
8. & \(E \rightarrow E < E\)                          & 17. & \(E \rightarrow i\) \\
9. & \(E \rightarrow E\mathbin{\texttt{<=}}E\)        &     & \\
\end{tabular}
\end{center}
\endgroup
```
