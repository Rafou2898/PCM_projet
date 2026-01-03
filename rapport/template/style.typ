#let font_color = rgb(50, 50, 50)
#let header_and_footer_color = rgb("#afafaf")
#let font = "Liberation Serif"
#let font_size = 11pt
#let bold_value = "bold"
#let emphasis_color = rgb("#e0e0e0")


#let apply_style(contenu) = {
  set text(font: font, size: font_size, fill: font_color)
  set par(justify: true)
  set quote(block: true)

  show quote: it => [
    #set quote(block: true)
    #block(breakable: false)[
      #align(left)[
        #text(font: font, size: font_size, fill: rgb("#6d6c6c"), style: "italic")[
          #it
        ]
      ]
    ]
  ]

  show raw: it => [
    #if (it.block) {
      let block_width = 100%
      let inset = 1em
      block(
        radius: 0.5em,
        fill: rgb("#e6e6e6"),
        inset: inset,
        width: block_width,
      )[
        #if (it.block and it.lang != none) {
          place(right + top, dy: -1.5em, dx: -1em)[
            #box(
              text(it.lang, size: 6pt, weight: bold_value),
              inset: 0.4em,
              stroke: rgb("#888888"),
              radius: 0.3em,
              fill: rgb("#ffffff"),
            )
          ]
        }
        #it
      ]
    } else {
      text(weight: "bold")[
        #it
      ]
    }
  ]

  set heading(numbering: "1.1.1.1.")

   show heading.where(level: 1): it => {
    set text(font: font, size: 18pt, weight: bold_value, fill: font_color)
    it
    v(1em)
  }


  show heading.where(level: 2): it => [
    #v(1em)
    #set text(font: font, size: 15pt, weight: bold_value, fill: font_color)
    #it
    #v(0.5em)
  ]

  show heading.where(level: 3): it => [
    #v(1em)
    #set text(font: font, size: 13pt, weight: bold_value, fill: font_color)
    #it
    #v(0.5em)
  ]

  show heading.where(level: 4): it => [
    #v(1em)
    #set text(font: font, size: font_size, weight: bold_value, fill: font_color)
    #it
    #v(0.5em)
  ]

  show strong: it => [
    #set text(font: font, weight: bold_value)
    #it
  ]

  contenu
}

#let emphasis(content) = {
  set text(font: font, style: "italic")
  rect(
    content,
    fill: emphasis_color,
    radius: 5pt,
  )
}

