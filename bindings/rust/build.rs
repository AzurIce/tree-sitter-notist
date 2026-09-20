fn main() {
    for (directory, library) in [
        ("src", "tree-sitter-notist"),
        ("notist-code/src", "tree-sitter-notist-code"),
    ] {
        let source_directory = std::path::Path::new(directory);
        let parser_path = source_directory.join("parser.c");
        let scanner_path = source_directory.join("scanner.c");
        let mut configuration = cc::Build::new();
        configuration.std("c11").include(source_directory);
        #[cfg(target_env = "msvc")]
        configuration.flag("-utf-8");
        configuration.file(&parser_path).file(&scanner_path);
        configuration.compile(library);
        println!("cargo:rerun-if-changed={}", parser_path.display());
        println!("cargo:rerun-if-changed={}", scanner_path.display());
        println!("cargo:rerun-if-changed={directory}/tree_sitter/parser.h");
    }
}
