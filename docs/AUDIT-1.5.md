# Audyt JERZY AUTO TUNE 1.5.0 przed przebudową 2.0

Baza: 48207fdcd3845c83ad9a1c29f9cad38faced3dcc, main. Początkowy checkout czysty.
Przejrzano PluginProcessor.cpp/.h, PluginEditor.cpp/.h, JerzyVSTGuiKit.h, CMakeLists.txt, README, tests/autotune_test.cpp, oba workflow oraz historię zmian.

## Elementy do zachowania

- JUCE 8.0.6 i Signalsmith Stretch przypięty do a670068d9aeb64913331d5cc29337b19a457a7df.
- Tożsamość VST3: JzAu/JzB4, audio.jerzy.autotune.v14; jej zmiana zerwałaby rozpoznawanie wtyczki w sesjach.
- Wszystkie 60 ID parametrów, kolejność, zakresy, wartości domyślne i typ stanu PARAMETERS. Nowe parametry należy dopisywać, z jawną migracją zachowania starszych sesji.
- Bufory przygotowywane w prepareToPlay, wyrównanie opóźnienia dry/wet, obsługa mono/stereo, maska nut przecinana ze skalą.
- Skalowalna powierzchnia GUI, attachments APVTS, gesty hosta, edycja liczb, pamięć rozmiaru okna i testy hit targets.
- Moduły bramki, ekspandera/HPF, de-essera, saturacji, kompresora i EQ jako baza do dalszych pomiarów.

## Ustalenia DSP

1. PluginProcessor.cpp:226: zakres lagów 44–850 daje około 51.9–1002 Hz przy 44.1 kHz, 56.5–1091 Hz przy 48 kHz i 112.9–2182 Hz przy 96 kHz. Stałe okno 4096 i krok analizy 512 także zmieniają czas działania wraz z fs.
2. Linie 241–265: różnica znormalizowana energią, próbkowanie co ósmy punkt, pierwszy lokalny minim poniżej progu. Nie jest to pełny YIN CMNDF. Brakuje oceny kandydatów harmonicznych, antyaliasingu przy redukcji analizy i testów missing fundamental.
3. Detektor ma tylko próg energii i progi dopasowania. Brakuje publicznej confidence, klasyfikacji voiced/unvoiced, histerezy oraz czasowego śledzenia nut. Skok >7 półtonów jest akceptowany od razu.
4. tunedRatio wybiera najbliższą nutę przy każdej analizie. Nie ma osobnego modelu centrum nuty, vibrato i przejść. Speed wygładza współczynnik przesunięcia; nie jest niezależną kontrolą ekspresji.
5. Wywołania setTransposeFactor nie konfigurują jawnie formantów. Ostateczny zakres wsparcia należy potwierdzić w przypiętej wersji biblioteki, a zachowanie mierzyć na sygnałach z kontrolowanymi rezonansami.
6. Linie 500–509: tanh przed EQ i outputGain nie gwarantuje końcowego ceiling. Potrzebny osobny limiter stereo-linked po wyjściu, z testami impulsów, release, opóźnienia i overshoot. Gwarancja sample peak nie oznacza gwarancji true peak.
7. Linie 456–459: całkowitoliczbowe delay taps powodują skokowe odczyty. Potrzebna interpolacja ułamkowa i testy widma modulacji. Detune jest nominalną głębokością modulacji, a nie stałym przesunięciem w centach.
8. Moduły są przełączane warunkami bool bez crossfade; ich stany poza kompresorem/doublerem mogą zastygać przy bypassie. Potrzebne ciągłe przetwarzanie stanów oraz wygładzane przejścia wet/dry. Dokładna specyfikacja użytkownika urwała się w tym punkcie.
9. Linie 524–531: suma L+R może wyzerować sygnał przeciwnofazowy dla detektora, choć miernik szczytowy poprawnie mierzy oba kanały.
10. Nie wszystkie parametry DSP są wygładzane; nagłe zmiany gain, threshold i delay mogą powodować artefakty. Współczynniki EQ są interpolowane bez osobnego dowodu stabilności przy skrajnych automatyzacjach.
11. Ogon deklarowany jako 0.10 s wymaga pomiaru względem rzeczywistego opóźnienia i wygasania pitch shiftera, szczególnie przy dużych fs.
12. Analiza całego bufora odbywa się przed przetwarzaniem jego fragmentów: trajektoria korekcji może zależeć od rozmiaru bloku. Potrzebne porównanie bloków 1/32/64/256/1024 i bloków zmiennej długości.

## GUI i ergonomia

- PluginEditor.cpp:357–358: paintChassis jest natychmiast przykrywany fillAll(outer). Używa też lokalnego rozmiaru edytora zamiast stałej powierzchni projektowej 1120×780.
- Zestaw Steel pozostaje w kodzie, lecz wiele kolorów i rysowanie pokręteł nadpisano lokalnie; wizualny język nie jest spójny z zestawem.
- Obecne GUI zapewnia dostęp do istniejących parametrów, ale nie ma podglądu confidence, centrum/target nuty, historii strojenia ani trybów zaawansowanej korekcji.
- Nie należy dodawać kontrolek bez działającego DSP i automatyzacji.

## Testy i CI

- Jedyny test CTest sprawdza GUI, wszystkie attachments, skalowanie, snapshoty, lifecycle, zapis/odczyt stanu, skończoność próbek, detekcję 447 Hz przy 48 kHz, pustą maskę i miernik stereo.
- Nie ma pomiarów wyjściowej częstotliwości, błędu w centach, zachowania harmoników, vibrato, formantów, bypassów, ceiling, latencji impulsowej ani budżetu czasu callbacku.
- build-windows.yml kompiluje Windows x64 VST3 i test, uruchamia CTest, kontroluje ciągi tekstowe oraz moduleinfo 1.5.0 i pakuje artefakt. Same ciągi nie dowodzą jakości DSP.
- gui-family-windows.yml tylko kompiluje VST3, bez regresji.
- Historia ostatnich zmian koncentruje się na GUI i CI; ostatni commit 48207fd zmienia jedynie workflow. DSP pochodzi z importu 1.4 i integracji 1.5.

## Warunki przyjęcia kolejnych zmian

Najpierw utrwalić bazowe wyniki. Każdą zmianę silnika sprawdzać dla 44.1/48/88.2/96/192 kHz, mono/stereo, różnych długości bloków, ciszy, szumu, impulsów i sygnałów harmonicznych. Zestawić jakość i CPU na rzeczywistych nagraniach wokalnych. Zachować osobne testy recall starego stanu i hostowej tożsamości. Windows 10/11 x64, FL Studio, automatyzacja, kompensacja opóźnienia, render offline oraz DPI 100/125/150% wymagają platformy docelowej. Kompilacja linuksowa nie zastępuje tych testów.

Ten dokument jest audytem bazowej wersji, nie potwierdzeniem gotowości wersji 2.0.

## Wynik uruchomienia bazowej wersji

Linux x64: CMake Release, JUCE 8.0.6, GCC 14.2.0. Oba cele JerzyAutoTune_VST3 i JerzyAutoTuneTests zbudowane poprawnie. Bazowy CTest autotune_gui_and_parameters: PASS (1/1, 0.83 s) pod Xvfb. Wygenerowano moduleinfo.json i snapshoty GUI. Nie jest to test Windows ani FL Studio.
