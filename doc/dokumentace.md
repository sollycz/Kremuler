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