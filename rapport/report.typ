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
= Mesures
== Tailles cutoff
= Profiling
= Optimisations possibles
= Problèmes rencontrés
= Conclusion