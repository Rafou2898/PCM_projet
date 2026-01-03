#import "style.typ": apply_style
#import "VAR.typ": *
#let header_and_footer_color = rgb("#afafaf")
#let header_and_footer_font_size = 8pt


#let apply_template(
  content,
  //lab_num: 1,
  course: "COURSE",
  Department: "DEPARTMENT",
  professor: "PROFESSOR",
  //assistant: "ASSISTANT",
  author: "AUTHOR",
  lab_title: "TITLE",
  date: "DD.MM.YYYY",
  classroom: "CLASSROOM",
) = {
  show: apply_style
  show link: underline

  // Title page ---------------------------------------------
  place(
    top + left,
    image(
      image_path,
      width: 60%,
      fit: "contain",
    ),
  )

  place(
    center + horizon,
    block[
      #set text(size: 24pt, weight: "bold")
      #text[
        #lab_title \
      ]

      #set text(size: 18pt, weight: "bold")
      #text[
        #Department\
        Unité d'enseignement #course
      ]
    ],
  )

  place(
    bottom,
    block[
      Auteur: *#author* \
      Professeur: *#professor* \
      //assistant: *#assistant* \
      Cours: *#classroom* \
      Date: *#date*
    ],
  )

  // End of title page -----------------------------------------------

  set page(
    numbering: none,
    margin: (top: 3.5cm, bottom: 3.5cm, left: 2cm, right: 2cm),
    header: [
      #set text(header_and_footer_font_size, fill: header_and_footer_color)
      #grid(
        columns: (1fr, 1fr),
        image(image_path, width: 60%, fit: "contain"), align(right, course + "\n" + date),
      )
      #line(length: 100%, stroke: 0.5pt + header_and_footer_color)
    ],

    footer: context [

      #set text(header_and_footer_font_size, fill: header_and_footer_color)
      #let footer_left_part = (
        "Auteur: " + author + "\n" + "Professeur: " + professor + "\n" + "\n"
      )
      #let footer_right_part = lab_title + "\n" + "Cours: " + classroom
      #line(length: 100%, stroke: 0.5pt + header_and_footer_color)
      #grid(
        columns: (3fr, 1fr, 3fr),
        grid.cell(
          x: 0,
          align(left + top, footer_left_part),
        ),

        grid.cell(
          x: 1,
          align(center + top, counter(page).display("1/1", both: true)),
        ),
        grid.cell(
          x: 2,
          align(right + top, footer_right_part),
        )
      )
    ],
  )
  outline(title: [Table des matières])
  pagebreak()
  content
}

