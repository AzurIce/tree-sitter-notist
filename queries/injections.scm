(fenced_raw
  language: (fence_info) @injection.language
  body: (fence_content) @injection.content)

((call
  function: (qualified_name
    (identifier) @_function)
  arguments: (arguments
    (argument
      (named_argument
        name: (identifier) @_text_name
        value: (expression
          (string
            [(escaped_inline_string (string_content) @injection.content)
             (escaped_multiline_string (string_content) @injection.content)
             (raw_inline_string (string_content) @injection.content)
             (raw_multiline_string (string_content) @injection.content)]))))
    (argument
      (named_argument
        name: (identifier) @_lang_name
        value: (expression
          (string
            (escaped_inline_string
              (string_content) @injection.language)))))))
  (#eq? @_function "raw")
  (#any-of? @_text_name "text" "source")
  (#eq? @_lang_name "lang")
  (#set! injection.combined))

((call
  function: (qualified_name
    (identifier) @_function)
  arguments: (arguments
    (argument
      (named_argument
        name: (identifier) @_lang_name
        value: (expression
          (string
            (escaped_inline_string
              (string_content) @injection.language)))))
    (argument
      (named_argument
        name: (identifier) @_text_name
        value: (expression
          (string
            [(escaped_inline_string (string_content) @injection.content)
             (escaped_multiline_string (string_content) @injection.content)
             (raw_inline_string (string_content) @injection.content)
             (raw_multiline_string (string_content) @injection.content)]))))))
  (#eq? @_function "raw")
  (#eq? @_lang_name "lang")
  (#any-of? @_text_name "text" "source")
  (#set! injection.combined))
