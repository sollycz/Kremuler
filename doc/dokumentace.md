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

\newpage

# Generátor kódu
Generátor rekurzivně prochází AST od hlavního uzlu AST_PROGRAM. Podpůrně také využívá tabulku symbolů pro zjistění dodatečných informací o daných funkcích.
Výrazy jsou vyhodnocovány post-order průchodem na datovém zásobníku. Příkazy jsou pokud možno také vyhodnocovány na zásobníku, případně používají pomocné globální proměnné. Argumenty jsou předávány přes nově vytvořený lokální rámec pro volanou funkci, hodnota je pak vrácená přes datový zásobník.

\newpage

```{=latex}
\newgeometry{margin=12mm}
\begin{landscape}
```

# Blokové schéma překladače

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

# Lexikální analýza

## Implementace

Lexikální analyzátor (Scanner) čte zdrojový kód ze standardního vstupu, rozpoznává jednotlivé lexémy a předává je syntaktickému analyzátoru ve formě tokenů. Základem implementace je deterministický konečný automat.

- Scanner čte ze standardního vstupu v případě, kdy parser požádá o další token.
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

## Diagram lexikálního analyzátoru

```{=latex}
\thispagestyle{empty}
\noindent\makebox[\linewidth][c]{\includegraphics[width=\linewidth,height=0.91\textheight,keepaspectratio,trim=7mm 7mm 7mm 7mm,clip]{automat_hlavni_diagram.pdf}}\par
\vfill
{\centering\thepage\par}
\newpage
```

## Diagram interpolovaných řetězců

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

# Precedenční syntaktická analýza

## Implementace

Precedenční analyzátor získává tokeny prostřednictvím syntaktického analyzátoru (parseru) a zpracovává výrazy podle precedenční tabulky a redukčních pravidel.

- Využívá dynamicky alokovaný zásobník, na který ukládá terminály a výrazy.
- Parser nejprve zavolá precedenční analyzátor, o další tokeny už žádá prostřednictvím parseru precedenční analyzátor.
- Symbol dna zásobníku označuje $, zároveň je symbol $ označení pro konec výrazu.
- Nejvrchnější terminál na zásobníku se porovná s aktuálním symbolem na vstupu a podle jejich vztahu v precedenční tabulce se provede operace redukce nebo operace vložení vstupního symbolu na zásobník.
- Při redukcích se rovnou vytváří abstraktní syntaktický strom (AST).
- Kořen AST se po úspěšném zpracování předá zpět parseru.
- Pokud je místo operandu klíčové slovo if, tak precedenční analyzátor zavolá funkci parseru, která zpracuje celý výraz (if..then..else) a vrátí jeho AST, které se vloží jako výraz na zásobník.

Při chybě se zpracování výrazu ukončí a volajícímu se vrátí příslušný chybový kód.

## Pravidla redukce výrazů

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

## Precedenční tabulka

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



# Sémantická analýza
Sémantická analýza dostává od syntaktické analýzy abstraktní syntaktický strom a tabulku symbolů z main.c, ve které jsou předem zaregistrovány pouze vestavěné funkce. Během analýzy doplňuje do stromu typy uzlů a identifikátory proměnných, které následně využívá generátor kódu. Při chybě vypisuje hlášení na standartní chybový výstup a analýzu ukončuje.

## Průchod programem
Samotná analýza probíhá ve dvou průchodech, aby bylo možné volat funkce definované později.

 - **1. průchod:** Pro každou definici funkce se vkládají seznamy parametrů (rozšíření PARAMLISTS), návratové typy a případný příjemce (rozšíření EXTKW) do tabulky symbolů. Neuvedené návratové typy jsou poznamenány a musí se odvodit v druhém průchodu. Tabulka symbolů v této fázi dokáže odhalit redefinice funkcí, či *@main*. Zároveň se kontroluje signatura funkce *@main*. Po prvním průchodu se kontroluje existence funkce *@main*.

 - **2. průchod:** Program se prochází od začátku a kontrolují se těla funkcí. Pokud se při kontrole narazí na volání funkce, jejíž tělo ještě nebylo zkontrolováno, zkontroluje se nejprve tělo volané funkce pro odvození jejího návratového typu. Z tohoto důvodu udržuje tabulka symbolů stavy odvození. Hlavní smyčka takto zkontrolované funkce přeskakuje. Narazí-li se na volání funkce, jejíž kontrola právě probíhá, jedná se o rekurzi a všechny funkce v tomto cyklu musí mít explicitní návratový typ.

## Kontroly v tělech funkcí
Každý blok, větev if a tělo cyklu má vlastní rozsah platnosti. Parametry, příjemce a řídicí proměnná cyklu for jsou neměnné. Při kontrole těl funkcí se dbá na:

 - **Proměnné:** u definice musí být hodnota přiřaditelná do uvedeného typu, jinak se typ odvodí z hodnoty. U přiřazení musí proměnná existovat a být měnitelná.
 - **Podmínky:** podmínky pro if a while a stráže for cyklu musí být typu Boolean. Meze a krok cyklu for musí být typu Int a krok nesmí být nulový literál.
 - **Return:** return s hodnotou vyžaduje explicitní návratový typ a hodnota s ním musí být kompatibilní. Return bez hodnoty je povolen pouze ve funkci s explicitním návratovým typem Unit.
 - **Volání:** kontroluje se počet seznamů argumentů, počet argumentů v nich a jejich typy. U volání metody se funkce vyhledává podle typu příjemce. Parser nedokáže rozlišit proměnnou od volání funkce bez seznamu argumentů, proto se identifikátor, který neodpovídá žádné proměnné, přepíše na volání funkce.
 - **Výrazy:** probíhá kontrola typů operandů a správných operací nad nimi. Typ Int je implicitně převeden na Double, pokud je to potřeba.

## Vyhledávací struktura
Vyhledávací struktura je implementována variantou 2 (hashovací tabulka je implementována jako ADT a tabulka symbolů ji následně interně využívá) a stará se o udržování platných rozsahů, kontrolu redefinic funkcí a proměnných. Pro ukládání informací o jednotlivých funkcích je využívána hashovací tabulka, každý záznam funkce obsahuje zásobník jejích proměnných. Tato struktura se následně předává generátoru kódu.

\newpage

# Reference {.unnumbered}

1. Hash Functions. Online. Cse.yorku.ca/~oz/hash.html. B.r. Dostupné z: <http://www.cse.yorku.ca/~oz/hash.html>. [cit. 2026-11-26].
