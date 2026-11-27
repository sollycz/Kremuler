---
lang: cs
papersize: a4
geometry: margin=2.5cm
fontsize: 11pt
numbersections: true
mainfont: TeX Gyre Termes
mathfont: TeX Gyre Termes Math
header-includes:
  - \usepackage{graphicx}
  - \usepackage{array}
  - \usepackage{xcolor}
  - \usepackage{colortbl}
  - \usepackage{pdflscape}
  - \usepackage{tabularx}
  - \usepackage{longtable}
  - \usepackage{caption}
  - \captionsetup{labelfont=bf, labelsep=colon}
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
\textbf{Oleksandr Teslja} & (xtesljo00) & 282245 & 25 \% \\
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

Časový plán jsme si vytvořili hned po sestavení týmu na první schůzce. Obsahoval rozdělení etap vývoje, termíny pro jednotlivé ucelené celky překladače a časovou rezervu.
Plán jsme museli několikrát upravit, ale díky časové rezervě jsme vše stihli.


Na první schůzce jsme navrhli rozhraní AST pro propojení jednotlivých částí. V týmu jsme vytvořili dvojice, které si navzájem kontrolovaly kód.

## Rozdělení práce
 - Oleksandr Teslja
   - Syntaktický analyzátor s rekurzivním sestupem a abstraktní syntaktický strom.

 - David Zoblivý
   - Lexikální analyzátor a syntaktická precedenční analýza.

 - Filip Radulovič
   - Sémantická analýza a tabulka symbolů.

 - Tomáš Neckař
   - Generátor cílového kódu a vestavěných funkcí.

Na dokumentaci a testování jednotlivých částí v rámci kontroly kódu se podílel celý tým.

## Využití AI
Na začátku jsme se v týmu dohodli na pravidlech používání AI. LLM byly využívány k:

 - vysvětlení nejasných částí specifikace, popřípadě fungování jazyka Scala
 - generování testovacích skriptů na konkrétní testovací případy
 - zapojení do procesu kontroly kódu
 - vytvoření jednoduchého Makefile

\newpage

# Implementace překladače

## Popis modulů

```{=latex}
\begin{table}[h]
\centering
\renewcommand{\arraystretch}{1.3}
\begin{tabularx}{\textwidth}{|l|X|}
\hline
\textbf{Modul} & \textbf{Popis} \\ \hline\hline
\texttt{main.c} & Vstupní bod překladače, postupně volá jednotlivé části a při chybě vrátí návratový kód. \\ \hline
\texttt{token.h} & Struktura tokenu obsahující jeho typ, data, pozici ve zdrojovém kódu a odsazení řádku. \\ \hline
\texttt{types.h} & Výčet datových typů jazyka IFJ26, včetně pomocných hodnot pro sémantickou analýzu. \\ \hline
\texttt{scanner.c/.h} & Lexikální analyzátor. Zpracovává zdrojový kód pomocí deterministického konečného automatu a předává syntaktickému analyzátoru tokeny. \\ \hline
\texttt{scanner\_buffer.c/.h} & Dynamicky alokovaná vyrovnávací paměť lexikálního analyzátoru. Při zaplnění zdvojnásobí svoji kapacitu. \\ \hline
\texttt{parser.c/.h} & Syntaktický analyzátor. Implementován na principu rekurzivního sestupu, primárně dle LL(1) gramatiky. Sestavuje abstraktní syntaktický strom a výrazy předává na zpracování precedenčnímu analyzátoru. \\ \hline
\texttt{precedence\_parser.c/.h} & Precedenční analyzátor. Zpracovává výrazy pomocí precedenční tabulky a redukčních pravidel a vytváří AST. \\ \hline
\texttt{precedence\_stack.c/.h} & Dynamicky alokovaný zásobník precedenčního analyzátoru. Při zaplnění zdvojnásobí svoji kapacitu. \\ \hline
\texttt{ast.c/.h} & Abstraktní syntakticky strom. Slouží jako rozhraní mezi syntaktickou analýzou, sémantickou analýzou a generátorem kódu. \\ \hline
\texttt{hashtable.c/.h} & ADT obecné hashovací tabulky s otevřeným adresováním. Dle potřeby zvětšuje či přehashuje tabulku. \\ \hline
\texttt{symtable.c/.h} & ADT tabulky symbolů využívající hashovací tabulku a hashovací funkci djb2. Uchovává informace o funkcích a proměnných, stará se o rozsahy platnosti, zastínění proměnných a přetížení funkcí. \\ \hline
\texttt{semantic.c/.h} & Dvouprůchodový sémantický analyzátor. V prvním průchodu registruje všechny funkce, ve druhém kontroluje jejich těla. \\ \hline
\texttt{code\_generator.c/.h} & Generátor cílového kódu, který rekurzivním průchodem AST vypisuje na stdout program v IFJcode26 včetně vestavěných funkcí. \\ \hline
\end{tabularx}
\end{table}
```

\newpage

```{=latex}
\newgeometry{margin=12mm}
\begin{landscape}
```

## Blokové schéma překladače

```{=latex}
\thispagestyle{empty}
\begin{center}
\includegraphics[width=\linewidth,height=0.82\textheight,keepaspectratio]{blokove_schema.pdf}
\end{center}
\vfill
{\centering\thepage\par}
\end{landscape}
\restoregeometry
```

## Lexikální analýza

### Implementace

Lexikální analyzátor (scanner) čte zdrojový kód ze standardního vstupu, rozpoznává jednotlivé lexémy a předává je syntaktickému analyzátoru ve formě tokenů. Základem implementace je deterministický konečný automat.

- Scanner čte ze standardního vstupu pouze tehdy, když parser požádá o další token.
- Dynamicky alokovaná vyrovnávací paměť se používá k ukládání znaků identifikátorů, číselných a řetězcových literálů.
- Rozlišuje identifikátory od klíčových slov.
- Počítá úvodní mezery určující odsazení řádku. Zjištěné odsazení předává parseru jako součást tokenu. Také se v tokenech nachází informace o pozici ve zdrojovém kódu pro přesnější lokalizaci chyb.
- Hloubku vnořených komentářů sleduje pomocí čítače.
- Pro podporu vnořených interpolovaných řetězců používá zásobník stavů interpolace.

Při chybě scanner vypíše její popis na standardní chybový výstup a vrátí příslušný chybový kód.

```{=latex}
\newgeometry{margin=12mm}
\begin{landscape}
```

### Diagram lexikálního analyzátoru

```{=latex}
\thispagestyle{empty}
\noindent\makebox[\linewidth][c]{\includegraphics[width=\linewidth,height=0.91\textheight,keepaspectratio,trim=7mm 7mm 7mm 7mm,clip]{automat_hlavni_diagram.pdf}}\par
\vfill
{\centering\thepage\par}
\newpage
```

### Diagram interpolovaných řetězců

```{=latex}
\thispagestyle{empty}
\begin{center}
\includegraphics[width=\linewidth,height=0.82\textheight,keepaspectratio]{automat_interpolateds_diagram.pdf}
\end{center}
\vfill
{\centering\thepage\par}
\end{landscape}
\restoregeometry
```

## Syntaktická analýza

Syntaktický analyzátor (parser) byl implementován na principu rekurzivního sestupu podle LL(1) (výjimečně, pro případy začínající identifikátorem, LL(2)) gramatiky. Na zpracování odsazení byl použit zásobník. Lexikální analýza totiž vrací odsazení aktuálního řádku pro každý token, a nejen tehdy, kdy dochází ke změně. Danou kontrolu má tedy na starost parser, jenž při detekci změny hloubky odsazení vloží pomocí funkce `next_token` potřebný počet `INDENT` a `DEDENT` tokenu. V případě, že dojde k ukončení více bloků najednou, je potřeba opakovaně porovnávat odsazení a vkládat tokeny `DEDENT`, dokud odsazení opět neodpovídá vrcholu zásobníku odsazení.
Stejné balancování odsazení se provede i při ukončení souboru (tokenu EOF) pro bloky, které zůstanou otevřené na konci souboru.

Jednořádková těla větví `if/else` nebo cyklu `for` se při průchodu parserem zabalují do `AST_BLOCK`, takže sémantická analýza a generátor kódu mohou počítat s tím, že tělo větve je vždy blok.

Když syntaktická analýza narazí na jakýkoliv výraz (v gramatice `<expression>`), zavolá `parse_expression` z precedenčního analyzátoru. Precedence dostává stejnou strukturu `Parser` jako vlastní funkce syntaktické analýzy a využívá potom zpětně `next_token`, aby načítala další tokeny. Díky tomu se odsazení řeší pouze na jednom místě.
Když precedence čeká operand a dostane `if`, zavolá zpátky parserovu `parse_if_expression`. Parser a precedence se tak mohou navzájem rekurzivně volat.

### LL gramatika

```{=latex}
{\footnotesize\ttfamily
\begin{longtable}{r l >{\raggedright\arraybackslash}p{9.2cm}}
\hline \rmfamily\bfseries č. & \rmfamily\bfseries neterminál & \rmfamily\bfseries pravá strana \\ \hline \endhead
1. & \textless{}program\textgreater{} & $\rightarrow$ TOK\_KW\_IMPORT TOK\_ID TOK\_DOT TOK\_MULTIPLY TOK\_EOL \textless{}functions\textgreater{} TOK\_EOF \\
2. & \textless{}functions\textgreater{} & $\rightarrow$ \textless{}function\textgreater{} \textless{}functions\textgreater{} \\
3. & \textless{}functions\textgreater{} & $\rightarrow$ \textless{}function\_main\textgreater{} \textless{}functions\textgreater{} \\
4. & \textless{}functions\textgreater{} & $\rightarrow$ \textless{}extension\textgreater{} \textless{}functions\textgreater{} \\
5. & \textless{}functions\textgreater{} & $\rightarrow$ $\varepsilon$ \\
6. & \textless{}function\textgreater{} & $\rightarrow$ TOK\_KW\_DEF TOK\_ID \textless{}parameter\_list\textgreater{} \textless{}optional\_type\textgreater{} TOK\_ASSIGN \textless{}function\_body\textgreater{} \\
7. & \textless{}function\_main\textgreater{} & $\rightarrow$ TOK\_AT TOK\_ID"main" \textless{}optional\_eol\textgreater{} \textless{}function\textgreater{} \\
8. & \textless{}optional\_eol\textgreater{} & $\rightarrow$ TOK\_EOL \\
9. & \textless{}optional\_eol\textgreater{} & $\rightarrow$ $\varepsilon$ \\
10. & \textless{}parameter\_list\textgreater{} & $\rightarrow$ TOK\_LEFT\_PARENTHESIS \textless{}parameters\textgreater{} TOK\_RIGHT\_PARENTHESIS \textless{}parameter\_list\textgreater{} \\
11. & \textless{}parameter\_list\textgreater{} & $\rightarrow$ $\varepsilon$ \\
12. & \textless{}parameters\textgreater{} & $\rightarrow$ \textless{}parameter\textgreater{} \textless{}parameter\_next\textgreater{} \\
13. & \textless{}parameters\textgreater{} & $\rightarrow$ $\varepsilon$ \\
14. & \textless{}parameter\_next\textgreater{} & $\rightarrow$ TOK\_COMMA \textless{}parameter\textgreater{} \textless{}parameter\_next\textgreater{} \\
15. & \textless{}parameter\_next\textgreater{} & $\rightarrow$ $\varepsilon$ \\
16. & \textless{}parameter\textgreater{} & $\rightarrow$ TOK\_ID TOK\_COLON \textless{}type\textgreater{} \\
17. & \textless{}optional\_type\textgreater{} & $\rightarrow$ TOK\_COLON \textless{}type\textgreater{} \\
18. & \textless{}optional\_type\textgreater{} & $\rightarrow$ $\varepsilon$ \\
19. & \textless{}type\textgreater{} & $\rightarrow$ TOK\_KW\_INT \\
20. & \textless{}type\textgreater{} & $\rightarrow$ TOK\_KW\_STRING \\
21. & \textless{}type\textgreater{} & $\rightarrow$ TOK\_KW\_UNIT \\
22. & \textless{}type\textgreater{} & $\rightarrow$ TOK\_KW\_DOUBLE \\
23. & \textless{}type\textgreater{} & $\rightarrow$ TOK\_KW\_BOOLEAN \\
24. & \textless{}function\_body\textgreater{} & $\rightarrow$ \textless{}value\textgreater{} \textless{}function\_body\_end\textgreater{} \\
25. & \textless{}function\_body\textgreater{} & $\rightarrow$ TOK\_EOL \textless{}block\textgreater{} \\
26. & \textless{}function\_body\_end\textgreater{} & $\rightarrow$ TOK\_EOL \\
27. & \textless{}function\_body\_end\textgreater{} & $\rightarrow$ $\varepsilon$ \\
28. & \textless{}extension\textgreater{} & $\rightarrow$ TOK\_KW\_EXTENSION TOK\_LEFT\_PARENTHESIS \textless{}parameter\textgreater{} TOK\_RIGHT\_PARENTHESIS \textless{}extension\_end\textgreater{} \\
29. & \textless{}extension\_end\textgreater{} & $\rightarrow$ TOK\_EOL TOK\_INDENT \textless{}extension\_method\_list\textgreater{} TOK\_DEDENT \\
30. & \textless{}extension\_end\textgreater{} & $\rightarrow$ \textless{}function\textgreater{} \\
31. & \textless{}extension\_method\_list\textgreater{} & $\rightarrow$ \textless{}function\textgreater{} \textless{}extension\_method\_list\textgreater{} \\
32. & \textless{}extension\_method\_list\textgreater{} & $\rightarrow$ $\varepsilon$ \\
33. & \textless{}block\textgreater{} & $\rightarrow$ TOK\_INDENT \textless{}statement\textgreater{} \textless{}statement\_next\textgreater{} TOK\_DEDENT \\
34. & \textless{}statement\textgreater{} & $\rightarrow$ \textless{}statement\_simple\textgreater{} \textless{}statement\_end\textgreater{} \\
35. & \textless{}statement\textgreater{} & $\rightarrow$ \textless{}while\textgreater{} \\
36. & \textless{}statement\_next\textgreater{} & $\rightarrow$ \textless{}statement\textgreater{} \textless{}statement\_next\textgreater{} \\
37. & \textless{}statement\_next\textgreater{} & $\rightarrow$ $\varepsilon$ \\
38. & \textless{}statement\_end\textgreater{} & $\rightarrow$ TOK\_EOL \\
39. & \textless{}statement\_end\textgreater{} & $\rightarrow$ TOK\_SEMICOLON \\
40. & \textless{}statement\_end\textgreater{} & $\rightarrow$ TOK\_SEMICOLON TOK\_EOL \\
41. & \textless{}statement\_end\textgreater{} & $\rightarrow$ $\varepsilon$ \\
42. & \textless{}statement\_simple\textgreater{} & $\rightarrow$ \textless{}variable\textgreater{} \\
43. & \textless{}statement\_simple\textgreater{} & $\rightarrow$ \textless{}assign\textgreater{} \\
44. & \textless{}statement\_simple\textgreater{} & $\rightarrow$ \textless{}return\textgreater{} \\
45. & \textless{}statement\_simple\textgreater{} & $\rightarrow$ \textless{}expression\textgreater{} \\
46. & \textless{}statement\_simple\textgreater{} & $\rightarrow$ \textless{}if\textgreater{} \\
47. & \textless{}statement\_simple\textgreater{} & $\rightarrow$ \textless{}for\textgreater{} \\
48. & \textless{}variable\textgreater{} & $\rightarrow$ \textless{}variable\_type\textgreater{} TOK\_ID \textless{}optional\_type\textgreater{} TOK\_ASSIGN \textless{}value\textgreater{} \\
49. & \textless{}variable\_type\textgreater{} & $\rightarrow$ TOK\_KW\_VAR \\
50. & \textless{}variable\_type\textgreater{} & $\rightarrow$ TOK\_KW\_VAL \\
51. & \textless{}assign\textgreater{} & $\rightarrow$ TOK\_ID TOK\_ASSIGN \textless{}value\textgreater{} \\
52. & \textless{}return\textgreater{} & $\rightarrow$ TOK\_KW\_RETURN \textless{}optional\_value\textgreater{} \\
53. & \textless{}optional\_value\textgreater{} & $\rightarrow$ \textless{}value\textgreater{} \\
54. & \textless{}optional\_value\textgreater{} & $\rightarrow$ $\varepsilon$ \\
55. & \textless{}value\textgreater{} & $\rightarrow$ \textless{}if\_expression\textgreater{} \\
56. & \textless{}value\textgreater{} & $\rightarrow$ \textless{}expression\textgreater{} \\
57. & \textless{}if\textgreater{} & $\rightarrow$ TOK\_KW\_IF \textless{}expression\textgreater{} TOK\_KW\_THEN \textless{}branch\textgreater{} \textless{}else\textgreater{} \\
58. & \textless{}branch\textgreater{} & $\rightarrow$ TOK\_EOL \textless{}block\textgreater{} \\
59. & \textless{}branch\textgreater{} & $\rightarrow$ \textless{}statement\_simple\textgreater{} \\
60. & \textless{}else\textgreater{} & $\rightarrow$ TOK\_KW\_ELSE \textless{}else\_next\textgreater{} \\
61. & \textless{}else\textgreater{} & $\rightarrow$ $\varepsilon$ \\
62. & \textless{}else\_next\textgreater{} & $\rightarrow$ \textless{}branch\textgreater{} \\
63. & \textless{}else\_next\textgreater{} & $\rightarrow$ \textless{}if\textgreater{} \\
64. & \textless{}if\_expression\textgreater{} & $\rightarrow$ TOK\_KW\_IF \textless{}expression\textgreater{} TOK\_KW\_THEN \textless{}branch\textgreater{} TOK\_KW\_ELSE \textless{}else\_expression\textgreater{} \\
65. & \textless{}else\_expression\textgreater{} & $\rightarrow$ \textless{}if\_expression\textgreater{} \\
66. & \textless{}else\_expression\textgreater{} & $\rightarrow$ \textless{}branch\textgreater{} \\
67. & \textless{}while\textgreater{} & $\rightarrow$ TOK\_KW\_WHILE \textless{}expression\textgreater{} TOK\_KW\_DO TOK\_EOL \textless{}block\textgreater{} \\
68. & \textless{}for\textgreater{} & $\rightarrow$ TOK\_KW\_FOR \textless{}for\_start\textgreater{} TOK\_KW\_DO \textless{}branch\textgreater{} \\
69. & \textless{}for\_start\textgreater{} & $\rightarrow$ \textless{}for\_header\textgreater{} \\
70. & \textless{}for\_start\textgreater{} & $\rightarrow$ \textless{}for\_header\_multiline\textgreater{} \\
71. & \textless{}for\_header\textgreater{} & $\rightarrow$ \textless{}for\_generator\textgreater{} \textless{}for\_guard\_list\textgreater{} \\
72. & \textless{}for\_header\_multiline\textgreater{} & $\rightarrow$ TOK\_EOL TOK\_INDENT \textless{}for\_generator\textgreater{} TOK\_EOL \textless{}for\_guard\_list\_multiline\textgreater{} TOK\_DEDENT \\
73. & \textless{}for\_generator\textgreater{} & $\rightarrow$ TOK\_ID TOK\_CONTROL\_ARROW \textless{}expression\textgreater{} \textless{}to\_or\_until\textgreater{} \textless{}expression\textgreater{} \textless{}for\_optional\_by\textgreater{} \\
74. & \textless{}to\_or\_until\textgreater{} & $\rightarrow$ TOK\_ID"to" \\
75. & \textless{}to\_or\_until\textgreater{} & $\rightarrow$ TOK\_ID"until" \\
76. & \textless{}for\_optional\_by\textgreater{} & $\rightarrow$ TOK\_ID"by" \textless{}expression\textgreater{} \\
77. & \textless{}for\_optional\_by\textgreater{} & $\rightarrow$ $\varepsilon$ \\
78. & \textless{}for\_guard\_list\textgreater{} & $\rightarrow$ TOK\_KW\_IF \textless{}expression\textgreater{} \textless{}for\_guard\_list\textgreater{} \\
79. & \textless{}for\_guard\_list\textgreater{} & $\rightarrow$ $\varepsilon$ \\
80. & \textless{}for\_guard\_list\_multiline\textgreater{} & $\rightarrow$ TOK\_KW\_IF \textless{}expression\textgreater{} TOK\_EOL \textless{}for\_guard\_list\_multiline\textgreater{} \\
81. & \textless{}for\_guard\_list\_multiline\textgreater{} & $\rightarrow$ $\varepsilon$ \\
\hline
\end{longtable}}
```

### LL tabulka

```{=latex}
\begin{center}
\setlength{\tabcolsep}{1.5pt}
\renewcommand{\arraystretch}{1.15}
\resizebox{\linewidth}{!}{%
\begin{tabular}[b]{|l|c|c|c|c|c|c|c|c|c|c|c|c|c|c|c|c|c|c|c|c|c|c|c|c|c|c|c|c|c|c|c|c|c|c|c|c|c|}
\hline
 & \rotatebox{90}{\ttfamily TOK\_KW\_IMPORT } & \rotatebox{90}{\ttfamily TOK\_ID } & \rotatebox{90}{\ttfamily TOK\_DOT } & \rotatebox{90}{\ttfamily TOK\_MULTIPLY } & \rotatebox{90}{\ttfamily TOK\_EOL } & \rotatebox{90}{\ttfamily TOK\_EOF } & \rotatebox{90}{\ttfamily TOK\_KW\_DEF } & \rotatebox{90}{\ttfamily TOK\_ASSIGN } & \rotatebox{90}{\ttfamily TOK\_AT } & \rotatebox{90}{\ttfamily TOK\_ID"main" } & \rotatebox{90}{\ttfamily TOK\_LEFT\_PARENTHESIS } & \rotatebox{90}{\ttfamily TOK\_RIGHT\_PARENTHESIS } & \rotatebox{90}{\ttfamily TOK\_COMMA } & \rotatebox{90}{\ttfamily TOK\_COLON } & \rotatebox{90}{\ttfamily TOK\_KW\_INT } & \rotatebox{90}{\ttfamily TOK\_KW\_STRING } & \rotatebox{90}{\ttfamily TOK\_KW\_UNIT } & \rotatebox{90}{\ttfamily TOK\_KW\_DOUBLE } & \rotatebox{90}{\ttfamily TOK\_KW\_BOOLEAN } & \rotatebox{90}{\ttfamily TOK\_KW\_EXTENSION } & \rotatebox{90}{\ttfamily TOK\_INDENT } & \rotatebox{90}{\ttfamily TOK\_DEDENT } & \rotatebox{90}{\ttfamily TOK\_SEMICOLON } & \rotatebox{90}{\ttfamily \textless{}expression\textgreater{} } & \rotatebox{90}{\ttfamily TOK\_KW\_VAR } & \rotatebox{90}{\ttfamily TOK\_KW\_VAL } & \rotatebox{90}{\ttfamily TOK\_KW\_RETURN } & \rotatebox{90}{\ttfamily TOK\_KW\_IF } & \rotatebox{90}{\ttfamily TOK\_KW\_THEN } & \rotatebox{90}{\ttfamily TOK\_KW\_ELSE } & \rotatebox{90}{\ttfamily TOK\_KW\_WHILE } & \rotatebox{90}{\ttfamily TOK\_KW\_DO } & \rotatebox{90}{\ttfamily TOK\_KW\_FOR } & \rotatebox{90}{\ttfamily TOK\_CONTROL\_ARROW } & \rotatebox{90}{\ttfamily TOK\_ID"to" } & \rotatebox{90}{\ttfamily TOK\_ID"until" } & \rotatebox{90}{\ttfamily TOK\_ID"by" } \\ \hline
{\ttfamily \textless{}program\textgreater{}} & 1 &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  \\ \hline
{\ttfamily \textless{}functions\textgreater{}} &  &  &  &  &  & 5 & 2 &  & 3 &  &  &  &  &  &  &  &  &  &  & 4 &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  \\ \hline
{\ttfamily \textless{}function\textgreater{}} &  &  &  &  &  &  & 6 &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  \\ \hline
{\ttfamily \textless{}function\_main\textgreater{}} &  &  &  &  &  &  &  &  & 7 &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  \\ \hline
{\ttfamily \textless{}optional\_eol\textgreater{}} &  &  &  &  & 8 &  & 9 &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  \\ \hline
{\ttfamily \textless{}parameter\_list\textgreater{}} &  &  &  &  &  &  &  & 11 &  &  & 10 &  &  & 11 &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  \\ \hline
{\ttfamily \textless{}parameters\textgreater{}} &  & 12 &  &  &  &  &  &  &  &  &  & 13 &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  \\ \hline
{\ttfamily \textless{}parameter\_next\textgreater{}} &  &  &  &  &  &  &  &  &  &  &  & 15 & 14 &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  \\ \hline
{\ttfamily \textless{}parameter\textgreater{}} &  & 16 &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  \\ \hline
{\ttfamily \textless{}optional\_type\textgreater{}} &  &  &  &  &  &  &  & 18 &  &  &  &  &  & 17 &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  \\ \hline
{\ttfamily \textless{}type\textgreater{}} &  &  &  &  &  &  &  &  &  &  &  &  &  &  & 19 & 20 & 21 & 22 & 23 &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  \\ \hline
{\ttfamily \textless{}function\_body\textgreater{}} &  & 24 &  &  & 25 &  &  &  &  &  & 24 &  &  &  &  &  &  &  &  &  &  &  &  & 24 &  &  &  & 24 &  &  &  &  &  &  &  &  &  \\ \hline
{\ttfamily \textless{}function\_body\_end\textgreater{}} &  &  &  &  & 26 & 27 & 27 &  & 27 &  &  &  &  &  &  &  &  &  &  & 27 &  & 27 &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  \\ \hline
{\ttfamily \textless{}extension\textgreater{}} &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  & 28 &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  \\ \hline
{\ttfamily \textless{}extension\_end\textgreater{}} &  &  &  &  & 29 &  & 30 &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  \\ \hline
{\ttfamily \textless{}extension\_method\_list\textgreater{}} &  &  &  &  &  &  & 31 &  &  &  &  &  &  &  &  &  &  &  &  &  &  & 32 &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  \\ \hline
{\ttfamily \textless{}block\textgreater{}} &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  & 33 &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  \\ \hline
{\ttfamily \textless{}statement\textgreater{}} &  & 34 &  &  &  &  &  &  &  &  & 34 &  &  &  &  &  &  &  &  &  &  &  &  & 34 & 34 & 34 & 34 & 34 &  &  & 35 &  & 34 &  &  &  &  \\ \hline
{\ttfamily \textless{}statement\_next\textgreater{}} &  & 36 &  &  &  &  &  &  &  &  & 36 &  &  &  &  &  &  &  &  &  &  & 37 &  & 36 & 36 & 36 & 36 & 36 &  &  & 36 &  & 36 &  &  &  &  \\ \hline
{\ttfamily \textless{}statement\_end\textgreater{}} &  & 41 &  &  & 38 &  &  &  &  &  & 41 &  &  &  &  &  &  &  &  &  &  & 41 & 39,40 & 41 & 41 & 41 & 41 & 41 &  &  & 41 &  & 41 &  &  &  &  \\ \hline
{\ttfamily \textless{}statement\_simple\textgreater{}} &  & 43,45 &  &  &  &  &  &  &  &  & 45 &  &  &  &  &  &  &  &  &  &  &  &  & 45 & 42 & 42 & 44 & 45,46 &  &  &  &  & 47 &  &  &  &  \\ \hline
{\ttfamily \textless{}variable\textgreater{}} &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  & 48 & 48 &  &  &  &  &  &  &  &  &  &  &  \\ \hline
{\ttfamily \textless{}variable\_type\textgreater{}} &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  & 49 & 50 &  &  &  &  &  &  &  &  &  &  &  \\ \hline
{\ttfamily \textless{}assign\textgreater{}} &  & 51 &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  \\ \hline
{\ttfamily \textless{}return\textgreater{}} &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  & 52 &  &  &  &  &  &  &  &  &  &  \\ \hline
{\ttfamily \textless{}optional\_value\textgreater{}} &  & 53,54 &  &  & 54 & 54 & 54 &  & 54 &  & 53,54 &  &  &  &  &  &  &  &  & 54 &  & 54 & 54 & 53,54 & 54 & 54 & 54 & 53,54 &  & 54 & 54 &  & 54 &  &  &  &  \\ \hline
{\ttfamily \textless{}value\textgreater{}} &  & 56 &  &  &  &  &  &  &  &  & 56 &  &  &  &  &  &  &  &  &  &  &  &  & 56 &  &  &  & 55,56 &  &  &  &  &  &  &  &  &  \\ \hline
{\ttfamily \textless{}if\textgreater{}} &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  & 57 &  &  &  &  &  &  &  &  &  \\ \hline
{\ttfamily \textless{}branch\textgreater{}} &  & 59 &  &  & 58 &  &  &  &  &  & 59 &  &  &  &  &  &  &  &  &  &  &  &  & 59 & 59 & 59 & 59 & 59 &  &  &  &  & 59 &  &  &  &  \\ \hline
{\ttfamily \textless{}else\textgreater{}} &  & 61 &  &  & 61 & 61 & 61 &  & 61 &  & 61 &  &  &  &  &  &  &  &  & 61 &  & 61 & 61 & 61 & 61 & 61 & 61 & 61 &  & 60,61 & 61 &  & 61 &  &  &  &  \\ \hline
{\ttfamily \textless{}else\_next\textgreater{}} &  & 62 &  &  & 62 &  &  &  &  &  & 62 &  &  &  &  &  &  &  &  &  &  &  &  & 62 & 62 & 62 & 62 & 62,63 &  &  &  &  & 62 &  &  &  &  \\ \hline
{\ttfamily \textless{}if\_expression\textgreater{}} &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  & 64 &  &  &  &  &  &  &  &  &  \\ \hline
{\ttfamily \textless{}else\_expression\textgreater{}} &  & 66 &  &  & 66 &  &  &  &  &  & 66 &  &  &  &  &  &  &  &  &  &  &  &  & 66 & 66 & 66 & 66 & 65,66 &  &  &  &  & 66 &  &  &  &  \\ \hline
{\ttfamily \textless{}while\textgreater{}} &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  & 67 &  &  &  &  &  &  \\ \hline
{\ttfamily \textless{}for\textgreater{}} &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  & 68 &  &  &  &  \\ \hline
{\ttfamily \textless{}for\_start\textgreater{}} &  & 69 &  &  & 70 &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  \\ \hline
{\ttfamily \textless{}for\_header\textgreater{}} &  & 71 &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  \\ \hline
{\ttfamily \textless{}for\_header\_multiline\textgreater{}} &  &  &  &  & 72 &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  \\ \hline
{\ttfamily \textless{}for\_generator\textgreater{}} &  & 73 &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  \\ \hline
{\ttfamily \textless{}to\_or\_until\textgreater{}} &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  & 74 & 75 &  \\ \hline
{\ttfamily \textless{}for\_optional\_by\textgreater{}} &  &  &  &  & 77 &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  & 77 &  &  &  & 77 &  &  &  &  & 76 \\ \hline
{\ttfamily \textless{}for\_guard\_list\textgreater{}} &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  & 78 &  &  &  & 79 &  &  &  &  &  \\ \hline
{\ttfamily \textless{}for\_guard\_list\_multiline\textgreater{}} &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  &  & 81 &  &  &  &  &  & 80 &  &  &  &  &  &  &  &  &  \\ \hline
\end{tabular}}
\end{center}
```

```{=latex}
{\footnotesize\itshape Pozn.: sloupec \texttt{\textless{}expression\textgreater{}} není technicky vzato terminál, takže v LL tabulce je jím myšlen „token, kterým může začínat výraz“.\par}
```

LL tabulka obsahuje několik LL(1) konfliktů - každý z nich parser řeší konkrétně způsobem popsaným níže:

1. `<statement_simple>` při `TOK_ID` může následovat přiřazení nebo výraz. Rozhoduje se podle dalšího tokenu `TOK_ASSIGN`, tedy LL(2).
2. Také LL(2) pro `TOK_SEMICOLON` - parser zkontroluje, zda za středníkem nenásleduje `TOK_EOL` - pokud ano, tak ho zkonzumuje.
3. `TOK_KW_IF` - Pro `<statement_simple>`, `<value>`, `<else_next>` a `<else_expression>` parser vždy zvolí `if` pravidlo.
4. `<optional_value>` - prázdný return se bere jen před `TOK_EOL`, `TOK_SEMICOLON` a `else`.
5. Dangling else v `<else>` patří vždy k nejbližšímu if.

### Precedenční syntaktická analýza

#### Implementace

Precedenční analyzátor získává tokeny prostřednictvím syntaktického analyzátoru (parseru) a zpracovává výrazy podle precedenční tabulky a redukčních pravidel.

- Využívá dynamicky alokovaný zásobník, na který ukládá terminály a výrazy.
- Parser předá řízení precedenčnímu analyzátoru, který si další tokeny vyžádá prostřednictvím parseru.
- Dno zásobníku označuje symbol $, který zároveň značí konec výrazu.
- Nejvrchnější terminál na zásobníku se porovná s aktuálním symbolem na vstupu a podle jejich vztahu v precedenční tabulce se provede operace redukce nebo operace vložení vstupního symbolu na zásobník.
- Při redukcích se rovnou vytváří abstraktní syntaktický strom (AST).
- Kořen AST se po úspěšném zpracování předá zpět parseru.
- Pokud je místo operandu klíčové slovo `if`, tak precedenční analyzátor zavolá funkci parseru, která zpracuje celý výraz `if ... then ... else` a vrátí jeho AST, který se vloží jako výraz na zásobník.

Při chybě se zpracování výrazu ukončí a volajícímu se vrátí příslušný chybový kód.

#### Pravidla redukce výrazů

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
\newpage

#### Precedenční tabulka

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

## Sémantická analýza
Sémantická analýza dostává od syntaktické analýzy abstraktní syntaktický strom a tabulku symbolů z `main.c`, ve které jsou předem zaregistrovány pouze vestavěné funkce. Během analýzy doplňuje do stromu typy uzlů a identifikátory proměnných, které následně využívá generátor kódu. Při chybě vypisuje hlášení na standardní chybový výstup a analýzu ukončuje.

### Průchod programem
Samotná analýza probíhá ve dvou průchodech, aby bylo možné volat funkce definované později.

 - **1. průchod:** Pro každou definici funkce se vkládají seznamy parametrů (rozšíření PARAMLISTS), návratové typy a případný příjemce (rozšíření EXTKW) do tabulky symbolů. Neuvedené návratové typy jsou poznamenány a musí se odvodit v druhém průchodu. Tabulka symbolů v této fázi dokáže odhalit redefinici funkce, a to včetně `@main`. Zároveň se kontroluje signatura funkce `@main`. Po prvním průchodu se kontroluje existence funkce `@main`.

 - **2. průchod:** Program se prochází od začátku a kontrolují se těla funkcí. Pokud se při kontrole narazí na volání funkce, jejíž tělo ještě nebylo zkontrolováno, zkontroluje se nejprve tělo volané funkce pro odvození jejího návratového typu. Z tohoto důvodu udržuje tabulka symbolů stavy odvození. Hlavní smyčka takto zkontrolované funkce přeskakuje. Narazí-li se na volání funkce, jejíž kontrola právě probíhá, jedná se o rekurzi a všechny funkce v tomto cyklu musí mít explicitní návratový typ.

### Kontroly v tělech funkcí
Každý blok, větev if a tělo cyklu má vlastní rozsah platnosti. Parametry, příjemce a řídicí proměnná `for` cyklu jsou neměnné. Při kontrole těl funkcí se dbá na:

 - **Proměnné:** u definice musí být hodnota přiřaditelná do uvedeného typu, jinak se typ odvodí z hodnoty. U přiřazení musí proměnná existovat a být měnitelná.
 - **Podmínky:** podmínky pro if a while a stráže cyklu `for` musí být typu Boolean. Meze a krok cyklu `for` musí být typu Int a krok nesmí být nulový literál.
 - **Return:** return s hodnotou vyžaduje explicitní návratový typ a hodnota s ním musí být kompatibilní. Return bez hodnoty je povolen pouze ve funkci s explicitním návratovým typem Unit.
 - **Volání:** kontroluje se počet seznamů argumentů, počet argumentů v nich a jejich typy. U volání metody se funkce vyhledává podle typu příjemce. Parser nedokáže rozlišit proměnnou od volání funkce bez seznamu argumentů, proto se identifikátor, který neodpovídá žádné proměnné, přepíše na volání funkce.
 - **Výrazy:** probíhá kontrola typů operandů a správných operací nad nimi. Typ Int je implicitně převeden na Double, pokud je to potřeba.

### Vyhledávací struktura
Vyhledávací struktura je implementována variantou 2 (hashovací tabulka je implementována jako ADT a tabulka symbolů ji následně interně využívá) a stará se o udržování rozsahů platnosti, kontrolu redefinic funkcí a proměnných. Pro ukládání informací o jednotlivých funkcích je využívána hashovací tabulka, každý záznam funkce obsahuje zásobník jejích proměnných. Tato struktura se následně předává generátoru kódu.

\newpage

## Generátor kódu
Generátor rekurzivně prochází AST od hlavního uzlu `AST_PROGRAM`. Podpůrně také využívá tabulku symbolů pro zjištění dodatečných informací o daných funkcích. Na začátku definuje pomocné globální proměnné a poté zavolá funkci označenou anotací `@main`. Zároveň generuje vestavěné funkce, které připojí na konec každého programu.

### Vyhodnocování
Výrazy (literály, proměnné, operace, volání) jsou vyhodnocovány post-order průchodem na datovém zásobníku. Příkazy (podmínky, cykly, přiřazení, návrat...) vyhodnocené výrazy z datového zásobníku odebírají a buďto je uloží do proměnných, a nebo se podle nich rozhodují.

Hodnoty výrazů použitých samostatně jako příkaz (volání funkce, `Unit` literál značící prázdný blok...) je nutné ze zásobníku specificky odstranit, aby na něm nezůstala nepoužitá data a nekazila další vyhodnocování výrazů. To ale neplatí v případech, kdy je nutné vrátit hodnotu na konci určitých bloků (implicitní návratová hodnota funkce nebo `if` výrazu).

### Konvence volání
Volající postupně vyhodnotí všechny argumenty na datový zásobník, vytvoří nový dočasný rámec, v němž definuje parametry volané funkce jako proměnné, a v opačném pořadí ze zásobníku přiřadí hodnoty do parametrů. Dočasný rámec přesune na vrchol zásobníku rámců, zavolá danou funkci a po návratu z funkce odebere rámec volané funkce z vrcholu zásobníku. Návratová hodnota je předána na vrchol datového zásobníku pro další použití.

\newpage

## Rozšíření

### FUNEXP
Rozšíření umožňuje volání funkcí uvnitř výrazů. Identifikátor následovaný otevírací závorkou se zpracuje jako název funkce uzlu `AST_CALL`. Argumenty pak zpracuje precedenční analýza a uloží do uzlu. Precedenční analyzátor pak pokračuje, dokud nezpracuje celý výraz jako obvykle.

### BOOLTHEN
Rozšíření umožňuje pracovat s pravdivostními hodnotami. Lexikální analýza rozpoznává nové tokeny a klíčová slova `true` a `false`. Precedenční analyzátor pracuje s novými operátory podle pravidel asociativity a priorit operátorů. Syntaktický analyzátor přidává typ Boolean a upravuje LL gramatiku tak, aby umožňovala práci s booleovskými hodnotami.

### CONTROL
Rozšíření přidává cyklus `for` a možnost používat `if` jako výraz. Lexikální analýza rozpoznává nová klíčová slova a tokeny. Pokud precedenční analyzátor očekává operand a dostane `if`, zavolá syntaktickou analýzu, která výraz `if ... then ... else` zpracuje a vrátí jako AST, se kterým precedenční analýza dále pracuje.

Parser při zpracování ifu jako výrazu kontroluje, že každý if v řetězci else if má větev else, jinak jde o syntaktickou chybu. Takto zpracovaný if i ify na konci větví následně označí příznakem `is_expression`. Pro případy, kde parser nestačí, se o dané označení postará až sémantická analýza. Variantu větví s jednořádkovým tělem parser zabalí do uzlu `AST_BLOCK`, takže sémantická analýza i generátor kódu vždy pracují už pouze s blokem.

Cyklus `for` dokáže parser zpracovat v jednořádkovém i víceřádkovém tvaru. Slova `to`, `until` a `by` nejsou klíčová slova, parser je rozpoznává pouze jako identifikátory s daným jménem.
Do uzlu `AST_FOR` ukládá jméno řídicí proměnné, meze `from` a `to`, příznak `inclusive` (to = true, until = false), krok step, případně seznam podmínek stráží, a samotné tělo cyklu.

### VALMEM
Rozšíření umožňuje využití tečkové notace. Lexikální analyzátor rozpoznává, zda je tečka za číselným literálem desetinná tečka, nebo tečková notace. Precedenční analyzátor vytvoří uzel `AST_CALL`, jehož název funkce se vezme z identifikátoru za tečkou. Hodnota před tečkou se vezme jako argument a nastaví se příznak `is_member`.

### INTERPOLATEDS
Rozšíření se týká především lexikální analýzy a precedenční analýzy a umožňuje interpolované řetězce. Lexikální analyzátor čte řetězec tak, že běžný text vrací jako token řetězcového literálu, výrazy typu `$id` vrací jako token identifikátoru a `${výraz}` vrací jako běžné tokeny. Pro vnořené interpolace se využívá zásobník struktur implementovaný jako jednosměrně vázaný seznam. Precedenční analyzátor zpracuje výrazy uvnitř `${výraz}`, vloží hodnoty z `$id` a `${výraz}` do volání `toStr()` a jednotlivé části propojí, aby vrátil jeden ucelený výraz.

### EXTKW
Rozšíření umožňuje definovat vlastní funkce, které lze volat tečkovou notací. Precedenční analyzátor vytvoří uzel `AST_CALL`, jehož název funkce se vezme z identifikátoru za tečkou. Hodnota před tečkou se vezme jako argument, nastaví se příznak `is_member` a případně se zpracují také argumenty funkce.

Definice rozšíření zpracovává parser. Za klíčovým slovem extension následuje v závorkách právě jeden explicitně otypovaný parametr (příjemce). Za ním je buď na stejném řádku ještě funkce, nebo odsazený blok s jednou či více metodami. Každá metoda se zpracuje jako běžná funkce a do položky `receiver` dostane vlastní kopii parametru příjemce. Ten se neukládá do seznamu `parameters`, normální funkce mají `receiver = NULL`. Metody se přidávají přímo do seznamu funkcí programu, takže sémantická analýza ani generátor kódu nepotřebují nový typ uzlu.

### PARAMLISTS
Rozšíření umožňuje definice a volání funkcí bez seznamu parametrů nebo s více než jedním seznamem parametrů. Precedenční analyzátor zpracovává seznamy argumentů a do uzlu `AST_CALL` ukládá počet seznamů a počet argumentů každého seznamu.

Definice funkcí zpracovává parser, jenž cyklicky čte seznamy v závorkách, dokud přichází token "(" (`TOK_LEFT_PARENTHESIS`), a vkládá parametry do uzlu `AST_FUNCTION`. Ty se ukládají do jednoho plochého seznamu `parameters` a jejich počet v každém seznamu se zapisuje do pole `list_sizes`. Celkový počet seznamů je uveden v `list_count`, kde hodnota 0 znamená funkci bez závorek (stylu `def funkce1`) a např. 1 funkci s jedním seznamem parametrů.

\newpage

# Reference {.unnumbered}

1. Hash Functions. Online. Cse.yorku.ca/~oz/hash.html. B.r. Dostupné z: <http://www.cse.yorku.ca/~oz/hash.html>. [cit. 2026-11-26].
2. C reference. Online. Cppreference.com. Dostupné z: <https://www.cppreference.com/c>. [cit. 2026-11-26].
3. MEDUNA, Alexander a Roman LUKÁŠ. Formální jazyky a překladače. Přednáškové prezentace. [cit. 2026-11-27].
4. ONDRYÁŠ, Ondřej. IFJ 2026/27: Projekt – překladač jazyka IFJ26. Přednášková prezentace. [cit. 2026-11-27].
5. BURGETOVÁ, Ivana a Radek HRANICKÝ. Algoritmy – IAL 2026/27. Přednáškové prezentace. [cit. 2026-11-27].