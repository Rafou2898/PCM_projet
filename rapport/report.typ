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

#set page(numbering: "1", number-align: center)
#counter(page).update(1)
= Introduction

Ce projet a pour but de réaliser la parallélisation du problème du voyageur de commerce (TSP) sur une architecture multi-coeurs en utilisant concepts vus en cours et de trouver une structure de données adaptée pour la gestion des tâches parallèles inégales.

Le problème du voyageur de commerce avec un branch-and-bound permet une décomposition des tâches en parallèles, mais pose des défis importants en termes d’équilibrage de charge et de synchronisation.

Une première implémentation déjà fournit, fournit une version séquentielle directe du branch-and-bound pour le TSP. Cette version sert de base pour les comparaisons de performance avec les versions parallèles développées dans ce projet.

Ensuite, une parallèle utilisant des mutex a été faite et permet de faire des comparaisons avec la version finale. Elle permet de valider la décomposition du problème et de mettre en évidence les limites liées à la contention.

Finalement, la dernière implémentation repose sur une stratégie de work-stealing avec des deques locales, visant à réduire les coûts de synchronisation et à améliorer l’utilisation des coeurs disponibles.

Les performances sont évaluées à l’aide de tests de mesures performés sur la  Xeon Phi mais aussi d’outils de profiling afin d’analyser le speedup, l’efficience et les principaux goulots d’étranglement. Les résultats montrent que les gains dépendent fortement de la taille du problème, du paramètre de cutoff et des choix d’implémentation.

= Version avec mutex 
La première version de l’algorithme repose sur une exécution parallèle basée sur un partage global des tâches, protégé par un mutex. Chaque thread extrait une tâche depuis une structure de données commune, la traite, puis insère éventuellement de nouvelles sous-tâches générées lors de la décomposition du problème. 

Le problème d'une version avec mutex est que les sections critiques deviennent rapidement des goulots d'étranglement lorsque le nombre de threads augmente. Cela va réduire le gain de performance attendu de la concurrence. Cette version sert donc principalement de référence fonctionnelle et de point de comparaison pour évaluer les bénéfices des approches plus avancées comme celle de la version basée sur le work-stealing.

= Work-Stealing Deque Lev-Chase
Une structure de donnée spécique connue permet de gérer ce genre de problème: les work-stealing deques (double-ended queues) proposées par Lev et Chase. Cette structure est faite de tel sorte que chaque thread dispose de sa propre file de tâches locale. Les opérations push et pop sont effectuées par le thread propriétaire de la deque.
Le propriétaire va consommer ses tâches d'une extrémité de la deque,
tandis que les autres threads peuvent tenter de voler du travail depuis l’extrémité opposée lorsque leur propre deque est vide.

Cette approche permet de réduire fortement la contention par rapport à une structure de tâches globale protégée par mutex. La décomposition du problème est sensée être bien répartie entre les threads et l’équilibrage de charge est assuré dynamiquement par les opérations de vol de tâches. Le parallélisme est exploité plus efficacement, en particulier pour les sous-problèmes générés en profondeur lors de l’exploration de l’arbre de recherche du TSP.

== Choix d'implémentation

=== Work-Stealing Deque
Chaque deque est définie par deux indices atomiques :

- bottom, manipulé uniquement par le thread propriétaire (push et pop)
- top, partagé avec les threads voleurs (steal).


*Principe général:* Chaque worker possède une deque locale représentée par un tableau circulaire (CircularBuffer) indexé par deux compteurs monotones top et bottom. Le thread propriétaire est le seul à effectuer push et pop sur bottom, ce qui réduit la synchronisation sur le chemin "courant". Les autres threads ne font que steal, c’est-à-dire tenter de récupérer du travail du côté top. Le buffer est stocké via un pointeur atomique `_array`

*Structure CircularBuffer:n* Les accès du buffer se font avec un index modulo capacity. En cas de dépassement (bottom - top >= capacity), le propriétaire alloue un nouveau buffer de capacité doublée et copie les éléments encore valides entre top et bottom. Le pointeur \_array est ensuite mis à jour.

*push:* Il lit bottom et top, vérifie si un resize est nécessaire, écrit l’élément à l’indice bottom, puis met à jour bottom. Le point critique est l’ordre d'écriture de l’élément puis la publication de bottom. Sans cela, un voleur pourrait observer un bottom augmenté et tenter de lire une case encore non écrite. C’est pour cela qu'on utilise un atomic_thread_fence(memory_order_release) qui permet de garantir que les stores effectués sur le buffer sont visibles avant l’augmentation de bottom.

*pop:* Plusieurs cas apparaissent pour le pop. Si `top > bottom` alors la deque est vide. On restaure bottom (bottom + 1) et on retourne nullptr. Il n’y a pas de de problème de race condition car il n’y a aucun élément à rendre visible. Si le deque a plus d’un élément donc `top < bottom`, le propriétaire peut simplement lire l’élément à l’indice bottom et le retourner. Aucun CAS sur top n’est nécessaire car un stealer ne peut pas voler l’élément du bas, seulement du haut.
Finalement, le deque a exactement un élément donc `top == bottom`, le propriétaire et un voleur peuvent viser le même élément (le dernier). Le propriétaire lit l’élément puis tente un CAS sur \_top pour "réserver" cet élément (top -> top+1). Si le CAS échoue, cela signifie qu’un stealer a gagné la course et a déjà incrémenté top ainsi le propriétaire doit alors annuler et retourner nullptr. La barrière atomic_thread_fence(memory_order_seq_cst) avant la lecture de top a de nouveau pour but d'éviter des réordonnancements qui rendraient la comparaison top/bottom incohérente.

*steal:* Le voleur lit top, puis lit bottom, et ne tente un vol que si `top < bottom`. Il lit l’élément à l’indice top puis tente un CAS pour incrémenter top. Si le CAS réussit, le vol est validé et l’élément est retourné. Si le CAS échoue, un autre thread (un autre voleur ou le propriétaire dans le cas dernier élément) a modifié top, donc le vol est abandonné (nullptr). Ici encore, la fence seq_cst entre les lectures de top et bottom est utilisée pour réduire les scénarios où un thread verrait des valeurs "désynchronisées" de top/bottom à cause de réordonnancements.


=== Work-Stealing Runner
Le WorkStealingRunner gère l’exécution en parallèle des tâches TSP en s’appuyant sur une deque Lev–Chase par thread. 

*Initialisation et structure* Le runner alloue N deques indépendantes (une par worker) et lance N threads. La tâche racine est injectée dans la deque du thread 0, puis chaque worker exécute une boucle de travail. Le runner contient un compteur atomique `leaves_remaining` utilisé pour détecter la fin de l’exécution.

*Boucle principale d’un worker* À chaque itération, le thread tente d’abord un pop sur sa deque locale. Si un travail est disponible, il traite immédiatement la tâche. Si la deque locale est vide, le worker passe en mode “steal” et tente de voler une tâche sur une autre deque.

*Vol de tâche* Le choix de la victime se fait en construisant une liste des autres threads, puis en la mélangeant aléatoirement. Le worker itère ensuite sur cette liste et appelle `steal()` sur chaque deque jusqu’à obtenir une tâche, ou à constater qu’aucun vol n’est possible. 

*Traitement d’une tâche* La fonction `process_task` applique le schéma classique de base `split()` produit soit 0 sous-tâche, soit n sous-tâches. Si `n>0`, les sous-tâches sont poussées dans la deque locale du thread courant. Si `n==0`, la tâche est résolue via `solve()`.

*Terminaison par comptage de feuilles* Au lieu d’essayer de détecter la fin via un compteur de tâches actives et l’état des files, le runner suit le nombre de feuilles restantes à traiter dans l’arbre de recherche. L’algorithme initialise `leaves_remaining` au nombre `full() - 1` (qui est la taille du graphe - 1), puis, à chaque feuille résolue, décrémente `leaves_remaining` d'un nombre qui est stocké dans un tableau de factorielles pré-calculées. L’exécution s’arrête lorsque leaves_remaining atteint zéro. Cette approche a l’avantage d’être indépendante des états transitoires des deques (qui peuvent être vides “temporairement” sans que le calcul soit terminé), qui était la technique utilisé précédemment.


La performance des résultats obtenus vont dépendre du coût de split/solve (qui domine le temps CPU), de la fréquence des phases d’inactivité menant à des tentatives de vol, et du niveau d’équilibrage de charge induit par le cutoff et la stratégie de distribution des sous-tâches.

= Mesures
== Tailles cutoff
= Profiling
= Optimisations possibles
Stealing?
= Problèmes rencontrés
= Conclusion