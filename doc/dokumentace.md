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

Časový plán jsme si vytvořili hned po sestavení týmu na první schůzce. Obsahoval rozdělení etap vývoje, termínů pro jednotlivé ucelené celky překladače a časovou rezervu.
Plán jsme museli několikrát upravit, díky časové rezervě jsme vše stihli.


Na první schůzce bylo vymyšleno rozhraní AST, pro propojení jednotlivých částí. V týmu jsme vytvořili dvojice, které si navzájem kontrolovaly kód.

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
Na začátku jsme se v týmu dohodli na kultuře používání AI. LLM byly využívány na:

 - vysvětlení nejasných částí specifikace popřípadě funkčnosti jazyka Scala
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
\texttt{token.h} & \\ \hline
\texttt{types.h} & \\ \hline
\texttt{scanner.c/.h} & Lexikální analyzátor. Zpracovává zdrojový kód pomocí deterministického konečného automatu a předává syntaktickému analyzátoru tokeny. \\ \hline
\texttt{scanner\_buffer.c/.h} & Dynamicky alokovaná vyrovnávací paměť lexikálního analyzátoru. Při zaplnění zdvojnásobí svoji kapacitu. \\ \hline
\texttt{parser.c/.h} & \\ \hline
\texttt{precedence\_parser.c/.h} & Precedenční analyzátor. Zpracovává výrazy pomocí precedenční tabulky a redukčních pravidel a vytváří AST. \\ \hline
\texttt{precedence\_stack.c/.h} & Dynamicky alokovaný zásobník precedenčního analyzátoru. Při zaplnění zdvojnásobí svoji kapacitu. \\ \hline
\texttt{ast.c/.h} & \\ \hline
\texttt{hashtable.c/.h} & ADT obecné hashovací tabulky s otevřeným adresováním. Dle potřeby zvětšuje či přehashuje tabulku. \\ \hline
\texttt{symtable.c/.h} & ADT tabulky symbolů využívající hashovací tabulku a hashovací funkci djb2. Uchovává informace o funkcích a proměnných, stará se o rozsahy platnosti a přetížení funkcí či proměnných. \\ \hline
\texttt{semantic.c/.h} & Dvouprůchodový sémantický analyzátor. V prvním průchodu registruje všechny funkce, ve druhém kontroluje jejich těla. \\ \hline
\texttt{code\_generator.c/.h} & Generátor cílového kódu, který post-order průchodem AST vypisuje na stdout program v IFJcode26 včetně vestavěných funkcí. \\ \hline
\end{tabularx}
\caption{Popis modulů}
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

## Precedenční syntaktická analýza

### Implementace

Precedenční analyzátor získává tokeny prostřednictvím syntaktického analyzátoru (parseru) a zpracovává výrazy podle precedenční tabulky a redukčních pravidel.

- Využívá dynamicky alokovaný zásobník, na který ukládá terminály a výrazy.
- Parser nejprve zavolá precedenční analyzátor, o další tokeny už žádá prostřednictvím parseru precedenční analyzátor.
- Symbol dna zásobníku označuje $, zároveň je symbol $ označení pro konec výrazu.
- Nejvrchnější terminál na zásobníku se porovná s aktuálním symbolem na vstupu a podle jejich vztahu v precedenční tabulce se provede operace redukce nebo operace vložení vstupního symbolu na zásobník.
- Při redukcích se rovnou vytváří abstraktní syntaktický strom (AST).
- Kořen AST se po úspěšném zpracování předá zpět parseru.
- Pokud je místo operandu klíčové slovo if, tak precedenční analyzátor zavolá funkci parseru, která zpracuje celý výraz (if..then..else) a vrátí jeho AST, které se vloží jako výraz na zásobník.

Při chybě se zpracování výrazu ukončí a volajícímu se vrátí příslušný chybový kód.

### Pravidla redukce výrazů

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

### Precedenční tabulka

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
Sémantická analýza dostává od syntaktické analýzy abstraktní syntaktický strom a tabulku symbolů z main.c, ve které jsou předem zaregistrovány pouze vestavěné funkce. Během analýzy doplňuje do stromu typy uzlů a identifikátory proměnných, které následně využívá generátor kódu. Při chybě vypisuje hlášení na standartní chybový výstup a analýzu ukončuje.

### Průchod programem
Samotná analýza probíhá ve dvou průchodech, aby bylo možné volat funkce definované později.

 - **1. průchod:** Pro každou definici funkce se vkládají seznamy parametrů (rozšíření PARAMLISTS), návratové typy a případný příjemce (rozšíření EXTKW) do tabulky symbolů. Neuvedené návratové typy jsou poznamenány a musí se odvodit v druhém průchodu. Tabulka symbolů v této fázi dokáže odhalit redefinice funkcí, či *@main*. Zároveň se kontroluje signatura funkce *@main*. Po prvním průchodu se kontroluje existence funkce *@main*.

 - **2. průchod:** Program se prochází od začátku a kontrolují se těla funkcí. Pokud se při kontrole narazí na volání funkce, jejíž tělo ještě nebylo zkontrolováno, zkontroluje se nejprve tělo volané funkce pro odvození jejího návratového typu. Z tohoto důvodu udržuje tabulka symbolů stavy odvození. Hlavní smyčka takto zkontrolované funkce přeskakuje. Narazí-li se na volání funkce, jejíž kontrola právě probíhá, jedná se o rekurzi a všechny funkce v tomto cyklu musí mít explicitní návratový typ.

### Kontroly v tělech funkcí
Každý blok, větev if a tělo cyklu má vlastní rozsah platnosti. Parametry, příjemce a řídicí proměnná cyklu for jsou neměnné. Při kontrole těl funkcí se dbá na:

 - **Proměnné:** u definice musí být hodnota přiřaditelná do uvedeného typu, jinak se typ odvodí z hodnoty. U přiřazení musí proměnná existovat a být měnitelná.
 - **Podmínky:** podmínky pro if a while a stráže for cyklu musí být typu Boolean. Meze a krok cyklu for musí být typu Int a krok nesmí být nulový literál.
 - **Return:** return s hodnotou vyžaduje explicitní návratový typ a hodnota s ním musí být kompatibilní. Return bez hodnoty je povolen pouze ve funkci s explicitním návratovým typem Unit.
 - **Volání:** kontroluje se počet seznamů argumentů, počet argumentů v nich a jejich typy. U volání metody se funkce vyhledává podle typu příjemce. Parser nedokáže rozlišit proměnnou od volání funkce bez seznamu argumentů, proto se identifikátor, který neodpovídá žádné proměnné, přepíše na volání funkce.
 - **Výrazy:** probíhá kontrola typů operandů a správných operací nad nimi. Typ Int je implicitně převeden na Double, pokud je to potřeba.

### Vyhledávací struktura
Vyhledávací struktura je implementována variantou 2 (hashovací tabulka je implementována jako ADT a tabulka symbolů ji následně interně využívá) a stará se o udržování platných rozsahů, kontrolu redefinic funkcí a proměnných. Pro ukládání informací o jednotlivých funkcích je využívána hashovací tabulka, každý záznam funkce obsahuje zásobník jejích proměnných. Tato struktura se následně předává generátoru kódu.

\newpage

## Generátor kódu
Generátor rekurzivně prochází AST od hlavního uzlu `AST_PROGRAM`. Podpůrně také využívá tabulku symbolů pro zjištění dodatečných informací o daných funkcích. Na začátku definuje pomocné globální proměnné a poté zavolá funkci označenou anotací `@main`. Generuje zároveň také vestavěné funkce, které připojí ke každému programu na konec.

### Vyhodnocování
Výrazy (literály, proměnné, operace, volání) jsou vyhodnocovány post-order průchodem na datovém zásobníku. Příkazy (podmínky, cykly, přiřazení, návrat, ...) obecně z datového zásobníku berou/konzumují vyhodnocené výrazy a ty ukládají do proměnných/podle nich se rozhodují.

Vyhodnocené samotné výrazy na řádku (volání funkce, `Unit` literál značící prázdný blok, ...) se musí specificky odstranit ze zásobníku, aby na něm nezůstala nepoužitá data a nekazila tak další vyhodnocování výrazů. To ale neplatí v případech, kdy je nutné vrátit hodnotu na konci určitých bloků (implicitní návratová hodnota funkce/if výrazu).

### Volací konvence
Volající postupně vyhodnotí všechny argumenty na datový zásobník, vytvoří nový dočasný rámec, v němž definuje parametry volané funkce jako proměnné, a v opačném pořadí ze zásobníku přiřadí hodnoty do argumentových proměnných. Dočasný rámec přesune na vrchol zásobníku rámců, zavolá danou funkci a po návratu z funkce odebere rámec zavolané funkce z vrcholu zásobníku. Návratová hodnota je předána na vrchol datového zásobníku pro další použití.

\newpage

## Rozšíření

### FUNEXP
Rozšíření umožňuje volání funkcí uvnitř výrazů. Identifikátor následovaný otevírací závorkou se zpracuje jako název funkce uzlu `AST_CALL`. Argumenty pak zpracuje precedenční analýza a uloží do uzlu. Precedenční analyzátor pak pokračuje, dokud nezpracuje celý výraz jako obvykle. 

### VALMEM
Rozšíření umožňuje využití tečkové notace. Lexikální analyzátor rozpoznává, zda je tečka za číselným literálem desetinná tečka nebo tečková notace. Precedenční analyzátor vytvoří uzel `AST_CALL`, jehož název funkce se vezme z identifikátoru za tečkou. Hodnota před tečkou se vezme jako argument a nastaví se příznak `is_member`.

### INTERPOLATEDS
Rozšíření se týkalo především lexikální analýzy a precedenční analýzy a umožňuje interpolované řetězce. Lexikální analyzátor čte řetězec tak, že běžný text vrací jako token řetězcového literálu, výrazy typu `$id` vrací jako token identifikátoru a `${vyraz}` vrací jako běžné tokeny. Pro vnořené interpolace se využívá zásobník struktur implementovaný jako jednosměrně vázaný seznam. Precedenční analyzátor zpracuje výrazy uvnitř `${vyraz}`, vloží hodnoty z `$id` a `${vyraz}` do volání `toStr()` a jednotlivé části propojí, aby vrátil jeden ucelený výraz.

### PARAMLISTS
Rozšíření umožňuje definice a volání funkcí bez seznamu parametrů nebo s více jak jedním seznamem parametrů. Precedenční analyzátor zpracovává seznamy argumentů a do uzlu `AST_CALL` ukládá počet seznamů a počet argumentů každého seznamu.

### EXTKW
Rozšíření umožňuje vlastní definice funkcí využité v tečkové notaci. Precedenční analyzátor vytvoří uzel `AST_CALL`, jehož název funkce se vezme z identifikátoru za tečkou. Hodnota před tečkou se vezme jako argument, nastaví se příznak `is_member` a případně se zpracují také argumenty funkce.

### CONTROL
Rozšíření přidává cyklus `for` a možnost používat `if` jako výraz. Lexikální analýza rozpoznává nová klíčová slova a tokeny. Precedenční analyzátor, pokud očekává operand a dostane if, tak zavolá syntaktickou analýzu, která výraz `if...then...else` zpracuje a pošle zpět jako `AST`, se kterým precedenční analýza opět pracuje.

### BOOLTHEN
Rozšíření umožňuje pracovat s pravdivostními hodnotami. Lexikální analýza rozpoznává nové tokeny a klíčová slova `true` a `false`. Precedenční analyzátor pracuje s novými operátory podle pravidel asociativity a priorit operátorů.

\newpage

# Reference {.unnumbered}

1. Hash Functions. Online. Cse.yorku.ca/~oz/hash.html. B.r. Dostupné z: <http://www.cse.yorku.ca/~oz/hash.html>. [cit. 2026-11-26].
2. C reference. Online. Cppreference.com. Dostupné z: <https://www.cppreference.com/c>. [cit. 2026-11-26].
