fn main() {
    let source_directory = std::path::Path::new("src");
    let parser_path = source_directory.join("parser.c");
    let scanner_path = source_directory.join("scanner.c");

    let mut configuration = cc::Build::new();
    configuration.std("c11").include(source_directory);
    #[cfg(target_env = "msvc")]
    configuration.flag("-utf-8");
    configuration.file(&parser_path).file(&scanner_path);
    configuration.compile("tree-sitter-notist");

    println!("cargo:rerun-if-changed={}", parser_path.display());
    println!("cargo:rerun-if-changed={}", scanner_path.display());
}
