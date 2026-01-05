#import "template/template.typ": apply_template
#import "@preview/note-me:0.5.0": *


#show: apply_template.with(
  //lab_num: 1,
  course: "PCM",
  Department: "MSE - Informatique et Système de Communication",
  professor: "Pasin Marcelo",
  //assistant: "ASSISTANT",
  author: "Dousse Rafael & Le Gouic Gaspard",
  lab_title: "Projet Final",
  date: "02.01.2026",
  classroom: "PCM",
)

#set text(lang: "fr")
#set page(numbering: "1", number-align: center)
#counter(page).update(1)
= Introduction

Ce projet a pour but de réaliser la parallélisation du problème du voyageur de commerce (TSP) sur une architecture multi-coeurs en utilisant les concepts vus en cours et de trouver une structure de données adaptée pour la gestion des tâches parallèles inégales.

Le problème du voyageur de commerce avec un branch-and-bound permet une décomposition des tâches en parallèle, mais pose des défis importants en termes d’équilibrage de charge et de synchronisation.

Une première implémentation déjà fournie est une version séquentielle directe du branch-and-bound pour le TSP. Cette version sert de base pour les comparaisons de performance avec les versions parallèles développées dans ce projet.

Ensuite, une version parallèle utilisant des mutex a été faite et permet de faire des comparaisons avec la version finale. Elle permet de valider la décomposition du problème et de mettre en évidence les limites liées à la contention.

Finalement, la dernière implémentation repose sur une stratégie de work-stealing avec des deques locales, visant à réduire les coûts de synchronisation et à améliorer l’utilisation des cœurs disponibles.

Les performances sont évaluées à l’aide de tests de mesures effectués sur la Xeon Phi mais aussi d’outils de profiling afin d’analyser le speed-up, l’efficience et les principaux goulots d’étranglement. Les résultats montrent que les gains dépendent fortement de la taille du problème, du paramètre de cutoff et des choix d’implémentation.

= Versions avec mutex 
La première version de l’algorithme repose sur une exécution parallèle basée sur un partage global des tâches, protégé par un mutex. Chaque thread extrait une tâche depuis une structure de données commune de type LIFO, la traite, puis insère éventuellement de nouvelles sous-tâches générées lors de la décomposition du problème. 

Le problème d'une version avec mutex est que les sections critiques deviennent rapidement des goulots d'étranglement lorsque le nombre de threads augmente. Cela réduit le gain de performance attendu de la concurrence. Cette version sert donc principalement de référence fonctionnelle et de point de comparaison pour évaluer les bénéfices des approches plus avancées comme celle de la version basée sur le work-stealing.

Initialement, la condition d'arrêt se basait sur un décompte des tâches, une seconde version utilisant un décompte du nombre de feuilles avec une factorielle a aussi été réalisée.

= Work-Stealing Deque Lev-Chase
Une structure de données spécique connue permet de gérer ce genre de problème: les work-stealing deques (double-ended queues) proposées par Lev et Chase. Cette structure est faite de telle sorte que chaque thread dispose de sa propre file de tâches locale. Les opérations push et pop sont effectuées par le thread propriétaire de la deque.
Le propriétaire va consommer ses tâches d'une extrémité de la deque,
tandis que les autres threads peuvent tenter de voler du travail depuis l’extrémité opposée lorsque leur propre deque est vide.

Cette approche permet de réduire fortement la contention par rapport à une structure de tâches globale protégée par un mutex. La décomposition du problème est censée être bien répartie entre les threads et l’équilibrage de charge est assuré dynamiquement par les opérations de vol de tâches. Le parallélisme est exploité plus efficacement, en particulier pour les sous-problèmes générés en profondeur lors de l’exploration de l’arbre de recherche du TSP.

== Choix d'implémentation

=== Work-Stealing Deque
Chaque deque est définie par deux indices atomiques :

- bottom, manipulé uniquement par le thread propriétaire (push et pop)
- top, partagé avec les threads voleurs (steal).


*Principe général:* Chaque worker possède une deque locale représentée par un tableau circulaire (CircularBuffer) indexé par deux compteurs monotones top et bottom. Le thread propriétaire est le seul à effectuer push et pop sur bottom, ce qui réduit la synchronisation sur le chemin "courant". Les autres threads ne font que steal, c’est-à-dire tenter de récupérer du travail du côté top. Le buffer est stocké via un pointeur atomique `_array`

*Structure CircularBuffer:n* Les accès au buffer se font avec un index modulo capacity. En cas de dépassement (bottom - top >= capacity), le propriétaire alloue un nouveau buffer de capacité doublée et copie les éléments encore valides entre top et bottom. Le pointeur \_array est ensuite mis à jour.

*push:* Il lit bottom et top, vérifie si un resize est nécessaire, écrit l’élément à l’indice bottom, puis met à jour bottom. Le point critique est l’ordre d'écriture de l’élément puis la publication de bottom. Sans cela, un voleur pourrait observer un bottom augmenté et tenter de lire une case encore non écrite. C’est pour cela qu'on utilise un atomic_thread_fence(memory_order_release) qui permet de garantir que les stores effectués sur le buffer sont visibles avant l’augmentation de bottom.

*pop:* Plusieurs cas apparaissent pour le pop. Si `top > bottom` alors la deque est vide. On restaure bottom (bottom + 1) et on retourne nullptr. Il n’y a pas de problème de race condition car il n’y a aucun élément à rendre visible. Si le deque a plus d’un élément donc `top < bottom`, le propriétaire peut simplement lire l’élément à l’indice bottom et le retourner. Aucun CAS sur top n’est nécessaire car un stealer ne peut pas voler l’élément du bas, seulement celui du haut.
Finalement, la deque a exactement un élément donc `top == bottom`, le propriétaire et un voleur peuvent viser le même élément (le dernier). Le propriétaire lit l’élément puis tente un CAS sur `top` pour "réserver" cet élément (`top -> top+1`). Si le CAS échoue, cela signifie qu’un stealer a gagné la course et a déjà incrémenté top, ainsi le propriétaire doit alors annuler et retourner nullptr. La barrière atomic_thread_fence(memory_order_seq_cst) avant la lecture de top a de nouveau pour but d'éviter des réordonnancements qui rendraient la comparaison top/bottom incohérente.

*steal:* Le voleur lit top, puis lit bottom, et ne tente un vol que si `top < bottom`. Il lit l’élément à l’indice top puis tente un CAS pour incrémenter top. Si le CAS réussit, le vol est validé et l’élément est retourné. Si le CAS échoue, un autre thread (un autre voleur ou le propriétaire dans le cas du dernier élément) a modifié top, donc le vol est abandonné (nullptr). Ici encore, la fence seq_cst entre les lectures de top et bottom est utilisée pour réduire les scénarios où un thread verrait des valeurs "désynchronisées" de top/bottom à cause de réordonnancements.


=== Work-Stealing Runner
Le WorkStealingRunner gère l’exécution en parallèle des tâches TSP en s’appuyant sur une deque Lev–Chase par thread. 

*Initialisation et structure* Le runner alloue N deques indépendantes (une par worker) et lance N threads. La tâche racine est injectée dans la deque du thread 0, puis chaque worker exécute une boucle de travail. Le runner contient un compteur atomique `leaves_remaining` utilisé pour détecter la fin de l’exécution.

*Boucle principale d’un worker* À chaque itération, le thread tente d’abord un pop sur sa deque locale. Si un travail est disponible, il traite immédiatement la tâche. Si la deque locale est vide, le worker passe en mode “steal” et tente de voler une tâche sur une autre deque.

*Vol de tâche* Le choix de la victime se fait en construisant une liste des autres threads, puis en la mélangeant aléatoirement. Le worker itère ensuite sur cette liste et appelle `steal()` sur chaque deque jusqu’à obtenir une tâche, ou à constater qu’aucun vol n’est possible. 

*Traitement d’une tâche* La fonction `process_task` applique le schéma classique de base `split()` produit soit 0 sous-tâche, soit n sous-tâches. Si `n>0`, les sous-tâches sont poussées dans la deque locale du thread courant. Si `n==0`, la tâche est résolue via `solve()`.

*Terminaison par comptage de feuilles* Au lieu d’essayer de détecter la fin via un compteur de tâches actives et l’état des files, le runner suit le nombre de feuilles restantes à traiter dans l’arbre de recherche. L’algorithme initialise `leaves_remaining` au nombre factoriel de `full() - 1` (qui est la taille du graphe - 1), puis, à chaque feuille résolue, décrémente `leaves_remaining` d'un nombre qui est stocké dans un tableau de factorielles pré-calculées. L’exécution s’arrête lorsque `leaves_remaining` atteint zéro. Cette approche a l’avantage d’être indépendante des états transitoires des deques (qui peuvent être vides “temporairement” sans que le calcul soit terminé), ce qui était la technique utilisée précédemment.


La performance des résultats obtenus va dépendre du coût de `split`/`solve` (qui domine le temps CPU), de la fréquence des phases d’inactivité menant à des tentatives de vol, et du niveau d’équilibrage de charge induit par le cutoff et la stratégie de distribution des sous-tâches.

= Mesures et résultats

Nous avons effectué les mesures suivantes : l’impact du cutoff (temps d’exécution en fonction de la valeur du cutoff), la scalabilité (temps d’exécution en fonction du nombre de villes du problème), le speedup (accélération par rapport à la version directe en fonction du nombre de threads) ainsi que l’efficience (rapport du speedup au nombre de threads).

À l’exception de l’impact du cutoff, qui a été évalué uniquement pour la version utilisant une work-stealing deque ("worksteal"), chaque mesure a été réalisée pour les deux versions implémentant des mécanismes de mutex ("mutex task" et "mutex factorial"), ainsi que pour la version "worksteal". La version séquentielle (direct) n’a été mesurée que dans le cadre de l’étude de la scalabilité et a servi de référence pour le calcul du speedup. Les temps d’exécution présentés correspondent à des moyennes obtenues sur cinq exécutions. Les graphiques ont été produits à l’aide de la bibliothèque Python matplotlib.

== Impact du cutoff

L’impact du cutoff a été évalué en faisant varier sa valeur entre 1 et 13, puis en mesurant le temps d’exécution pour un problème comportant 14 villes, exécuté avec 60 threads. Les résultats obtenus sont les suivants :

#figure(
  image("template/img/Figure_1.png", width: 60%),
  caption: [
    Impact du cutoff : temps d'exécution en fonction du cutoff ("worksteal", 14 villes, 60 threads).
  ],
)

Comme on peut le constater, le cutoff optimal pour ce problème est égal à 8. Cette valeur sera donc conservée pour l’ensemble des autres mesures. Par ailleurs, ces résultats mettent en évidence l’impact significatif du cutoff sur le temps d’exécution. Ce comportement est attendu, dans la mesure où le cutoff définit la frontière entre la poursuite de la décomposition des tâches et leur résolution.
Une résolution située trop haut dans l’arbre n’exploite pas pleinement les bénéfices du multithreading, tandis qu’une résolution trop profonde dans l’arbre engendre un surcoût important lié à la gestion du multithreading.

== Scalabilité

La scalabilité a été évaluée en faisant varier le nombre de villes du problème entre 4 et 16, puis en mesurant le temps d’exécution avec un cutoff fixé à 8 et un total de 60 threads. Les résultats obtenus sont les suivants :

#figure(
  image("template/img/Figure_2.png", width: 60%),
  caption: [
    Scalabilité : temps d'exécution en fonction du nombre de villes (60 threads, cutoff 8).
  ],
)

Comme on peut le constater, pour un faible nombre de villes, les temps d’exécution très faibles semblent identiques pour toutes les versions. En réalité, il s’agit d’une illusion due à l’échelle linéaire du graphique et aux valeurs très réduites de temps, puisque la version directe reste en réalité plus rapide. En revanche, à partir de 14 villes, et de manière encore plus marquée pour 15 et 16 villes, l’avantage du multithreading devient nettement perceptible. Ceci s’accompagne d’une augmentation du temps d’exécution de la version directe, qui atteint plusieurs secondes, rendant la différence plus visible à notre échelle. C’est pourquoi les mesures de speedup et d’efficience ont été réalisées sur un problème comportant 15 villes : ce choix constitue un compromis pertinent entre la durée nécessaire à l’exécution des mesures (notamment pour la version séquentielle) et le gain observable grâce au multithreading.

Par ailleurs, nous avons également mesuré le temps d’exécution de notre meilleure version ("worksteal") pour un problème à 17 villes (93,2175 s), et avons tenté de réaliser la mesure pour un problème à 18 villes, mais celle-ci a été impossible en raison d’un dépassement de la RAM.

== Speedup et efficience

Les mesures de speedup et d’efficience ont été réalisées en mesurant le temps d’exécution des trois versions multithreadées, en faisant varier le nombre de threads de 1 à 256, avec des mesures effectuées toutes les dizaines. Le speedup et l’efficience ont ensuite été calculés en prenant comme référence le temps d’exécution de la version séquentielle, égal à 35,9119 s. Les résultats obtenus sont les suivants :

#figure(
  image("template/img/Figure_3.png", width: 60%),
  caption: [
    Speedup : accélération par rapport à la version directe (35,9119s) en fonction du nombre de threads (15 villes, cutoff 8).
  ],
)

#figure(
  image("template/img/Figure_4.png", width: 60%),
  caption: [
    Efficience : rapport du speedup au nombre de threads (15 villes, cutoff 8).
  ],
)

Comme on peut l’observer, le speedup de la version "worksteal" est nettement supérieur à celui des autres implémentations, avec un pic atteignant 38,5 pour 60 threads. Les versions "mutex task" et "mutex factorial" présentent des pics de speedup respectivement de 13,9 pour 120 threads et de 11,6 pour 60 threads.

De manière plus notable, les versions basées sur des mutex atteignent rapidement un plateau de speedup, ce qui se traduit par une diminution précoce de l’efficience. À l’inverse, la version "worksteal" conserve une efficience élevée pour des nombres de threads inférieurs à 50.

Ces observations confirment que, comme attendu, la problématique de contention sur les structures de données partagées est nettement plus marquée dans les versions utilisant des mutex que dans la version "worksteal". Cela se manifeste à la fois par de meilleures valeurs globales de speedup et par une efficience plus robuste face à l’augmentation du nombre de threads.

Néanmoins, les performances obtenues avec la version worksteal demeurent en deçà des attentes, compte tenu du fait que cette structure est spécifiquement conçue pour limiter la contention, celle-ci n’intervenant qu’au moment des opérations de steal. Ce constat nous a conduits à entreprendre une phase de profiling et de comptage d’opérations, afin d’identifier d’éventuels problèmes ou goulets d’étranglement et d’envisager des pistes d’optimisation.

= Profiling
Nous avons effectué un profiling du code pour voir où se situent les goulots d’étranglement. Le profiling a été réalisé à l’aide de `perf stat`, `perf record` et `Intel VTune`, sur l’instance dj38 avec 4 à 5 threads, afin d’identifier les goulots d’étranglement dominants et d’évaluer l’impact des premières optimisations.

Les mesures issues de `perf stat` indiquent une bonne localité mémoire avec une bonne utilisation des caches L1, LLC et TLB. Mais on peut quand même remarquer une activité de branchement élevée, avec un taux de mauvaises prédictions assez élevé qui est normal vu qu'on utilise un algorithme de type branch-and-bound conditionnel. 

Les outils `perf record` et VTune nous ont montré qu'une grande partie du travail se fait sur `TSPTask::solve()`, alors que l’infrastructure de work-stealing (runner et deque) n’apparaît pas comme un coût dominant en cycles. Les sous-arbres créés montrent de nombreux appels aux fonctions push et pop. Un grand nombre d'appels est aussi effectué par la fonction `size()` et les fonctions `set` et `test` du bitset. 

Suite à ce profiling, deux "optimisations" ont été faites et c'est la mise en cache de la taille du graphe et le remplacement de `std::bitset` par un masque binaire et l'utilisation d'opérations binaires pour remplacer set et test. Lors du lancement du programme avec un petit nombre de thread, nous n'avons pas observé une grande amélioration du temps (voir des résultats pire) mais avec beaucoup de threads on a une grosse amélioration du temps d'exécution.

Version avec 4 threads:
#set text(size: 8pt)

```
./tsp "./dj38.tsp" "--threads" "4" "--cutoff" "8" "--cities" "15

    CPU Time: 12.840s
Top Hotspots
Function                                                             Module  CPU Time  % of CPU Time(%)
-------------------------------------------------------------------  ------  --------  ----------------
TSPTask::solve                                                       tsp       2.230s             17.4%
TSPPath::pop                                                         tsp       1.710s             13.3%
std::bitset<(unsigned long)32>::test                                 tsp       1.420s             11.1%
std::bitset<(unsigned long)32>::set                                  tsp       1.280s             10.0%
std::vector<TSPGraph::Point, std::allocator<TSPGraph::Point>>::size  tsp       1.160s              9.0%
[Others]                                                             N/A       5.040s             39.3%
```
#set text(size: 11pt)

Version "optimisé" avec 4 threads :
#set text(size: 8pt)
```
./tsp "./dj38.tsp" "--threads" "4" "--cutoff" "8" "--cities" "15
  CPU Time: 19.119s
Top Hotspots
Function        Module  CPU Time  % of CPU Time(%)
--------------  ------  --------  ----------------
TSPTask::solve  tsp       1.981s             10.4%
TSPPath::push   tsp       1.869s              9.8%
TSPPath::push   tsp       1.560s              8.2%
TSPTask::solve  tsp       1.250s              6.5%
TSPPath::pop    tsp       1.158s              6.1%
[Others]        N/A      11.301s             59.1%
```
#set text(size: 11pt)

Version avec 12 threads:
#set text(size: 8pt)
```
./tsp "./dj38.tsp" "--threads" "12" "--cutoff" "8" "--cities" "16
    CPU Time: 209.430s
Top Hotspots
Function                                                             Module  CPU Time  % of CPU Time(%)
-------------------------------------------------------------------  ------  --------  ----------------
std::vector<TSPGraph::Point, std::allocator<TSPGraph::Point>>::size  tsp      60.891s             29.1%
std::bitset<(unsigned long)32>::set                                  tsp      49.226s             23.5%
std::vector<TSPGraph::Point, std::allocator<TSPGraph::Point>>::size  tsp      20.291s              9.7%
TSPTask::solve                                                       tsp      13.386s              6.4%
TSPPath::pop                                                         tsp      13.344s              6.4%
[Others]                                                             N/A      52.291s 
```
#set text(size: 11pt)

Version "optimisé" avec 12 threads:
#set text(size: 8pt)
```
./tsp "./dj38.tsp" "--threads" "12" "--cutoff" "8" "--cities" "16" 
   CPU Time: 86.479s
Top Hotspots
Function        Module          CPU Time  % of CPU Time(%)
--------------  --------------  --------  ----------------
TSPTask::solve  tsp               7.548s              8.7%
TSPPath::push   tsp               6.782s              7.8%
operator new    libstdc++.so.6    5.907s              6.8%
TSPPath::push   tsp               5.698s              6.6%
TSPTask::solve  tsp               5.404s              6.2%
[Others]        N/A              55.140s             63.8%

```
#set text(size: 11pt)


Finalement, on n'a pas tiré plus d'informations du profiling avec VTune ou perf record et surtout celui-ci a été effectué sur une machine personnelle et non pas sur la machine Xeon phi ce qui limite la pertinence des résultats. On voit que nous avons principalement des opérations liées au TSP et pas à l'infrastructure de work-stealing, c'est à dire que l'on voit peu de `steal()` dans le hotspot ce qui peut être un bon signe car ça veut dire que le vol de tâche n'est pas un goulot d'étranglement mais cela ne nous aide pas à comprendre pourquoi on n'a pas un meilleur speed-up.


== Instrumentation et compteurs internes

Une instrumentation légère a été ajoutée au runtime de work-stealing afin de collecter des statistiques internes par thread (nombre de push et pop locaux, tentatives et succès de vol, boucles à vide, nombre de tâches traitées). Ces compteurs, regroupés dans la structure `WSStats`, permettent en principe d’analyser plus finement le comportement du scheduler, le déséquilibre de charge et l’efficacité réelle du vol de travail.

Dans le cadre de ce projet, ces compteurs n’ont cependant pas vraiment été exploités car ajoutés à la fin. En l’état, ils ont principalement servi à vérifier que l'ensemble fonctionnait correctement.
Une extension possible serait d’ajouter des compteurs plus directement liés à l’algorithme, par exemple le nombre de noeuds explorés, de branches prunées, ou de feuilles résolues par thread et de cas réussis. Cela permettrait de relier précisément les statistiques du scheduler au comportement du branch-and-bound, et de mieux expliquer les limitations observées en termes de speed-up et d’équilibrage de charge.

== Optimisations et améliorations possibles

Cette section présente plusieurs pistes d’optimisation envisagées, ainsi que des améliorations qui auraient pu être apportées au projet.

== Fonction steal

Tout d’abord, les performances de la version "worksteal", inférieures aux attentes, ainsi que le nombre non négligeable de tentatives de *steal* infructueuses observées via les compteurs internes, suggèrent que cette fonction pourrait être optimisée. Il serait par exemple envisageable d’introduire un mécanisme d’exponential backoff afin de réduire la contention et le coût des tentatives de steal répétées.

== Gestion des TSPPath

Une autre piste d’optimisation concerne la gestion des objets TSPPath. Le profiling, bien qu’effectué avec un nombre restreint de threads, indique qu’une part non négligeable du temps d’exécution est consacrée à leur manipulation. Bien que des optimisations à ce niveau bénéficieraient également à la version séquentielle, leur rôle central dans la conservation du plus court chemin en fait un levier d’optimisation pertinent.

== Structures lock-free : pile et file

Parmi les améliorations envisageables, l’intégration d’une pile et d’une file lock-free aurait également été intéressante. Bien que nous estimions que ces implémentations auraient été moins performantes que la version "worksteal", elles auraient fourni des points de comparaison supplémentaires. Cela aurait notamment permis d’approfondir l’analyse des stratégies de parcours de l’arbre (en profondeur versus en largeur), ainsi que l’impact d’une contention accrue dans le cas d’une pile, liée aux opérations réalisées sur une même extrémité

== Profiling plus extensif

Nous aurions également souhaité réaliser un profiling plus approfondi, en particulier sur le serveur, avec un nombre plus élevé de threads. Cela aurait permis de mieux comprendre les causes précises de la perte d’efficience observée pour la version "worksteal", ainsi que d’identifier plus finement les phénomènes de contention associés.

== Structuration du code

Enfin, une structuration plus modulaire du code aurait facilité l’implémentation et l’évaluation de nouvelles approches, tant du point de vue des structures de données que des stratégies de parallélisation.

= Conclusion

Dans ce projet, nous avons abordé la résolution du problème du voyageur de commerce à l’aide d’une approche branch-and-bound. Notre travail a consisté à proposer une implémentation parallèle du problème, reposant sur une distribution du travail entre les threads au moyen d’une structure de données non bloquante.

Pour ce faire, trois versions ont été implémentées. Deux d’entre elles utilisent une pile globale protégée par un mutex, avec des variations dans les conditions d’arrêt, afin de servir de référence. La troisième version repose sur une Work-Stealing Deque de type Lev–Chase. Cette dernière, particulièrement adaptée au problème étudié, présente plusieurs avantages. D’un point de vue local, les opérations de push et pop réalisées au bas de la deque favorisent un parcours en profondeur, permettant d’obtenir plus rapidement des solutions et de réduire ainsi l’espace de recherche par un élagage plus efficace. D’un point de vue global, le mécanisme de vol de tâches lock-free au sommet de la deque assure une bonne répartition du travail entre les threads tout en minimisant la contention.

La version worksteal a permis d’atteindre un pic de speedup de 38,5 pour 60 threads. De plus, elle présente une meilleure résilience de l’efficience face à l’augmentation du nombre de threads, avec des valeurs particulièrement élevées pour des configurations inférieures à 50 threads.

Néanmoins, ces résultats restant en deçà de nos attentes, nous avons entrepris une phase de profiling et mis en place des compteurs internes afin d’identifier les sources potentielles de contention et les causes de la perte d’efficience observée. Cette analyse a permis de dégager plusieurs pistes d’optimisation, notamment au niveau de la fonction de steal et de la gestion des structures TSPPath.

Enfin, plusieurs améliorations pourraient être apportées au projet. L’intégration de piles et de files lock-free intermédiaires constituerait un enrichissement pertinent, non pas nécessairement pour améliorer les performances, mais pour approfondir l’analyse des stratégies de parcours de l’arbre de recherche. Un profiling plus extensif, en particulier sur le serveur avec un nombre plus important de threads, permettrait également de mieux interpréter les résultats obtenus. Enfin, une meilleure structuration du code faciliterait l’implémentation de nouvelles stratégies et la conduite de tests supplémentaires.

