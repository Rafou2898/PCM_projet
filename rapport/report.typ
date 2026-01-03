#import "template/template.typ": apply_template
#import "@preview/note-me:0.5.0": *


#show: apply_template.with(
  //lab_num: 1,
  course: "PCM",
  Department: "Informatique",
  professor: "Pasin Marcelo",
  //assistant: "ASSISTANT",
  author: "Dousse Rafael & Le Gouic Gaspard",
  lab_title: "Projet Final",
  date: "02.01.2026",
  classroom: "PCM",
)


= Introduction

= Version mutexer 

= Work-Stealing Deque Lev-Chase
== Choix d'implémentation

= Mesures

== Tailles cutoff

= Profiling
= Optimisations possibles
= Problèmes rencontrés
= Conclusion