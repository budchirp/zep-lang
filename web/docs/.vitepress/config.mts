import { defineConfig } from "vitepress";
import grammar from "../../../extensions/vscode/syntaxes/zep.tmLanguage.json" with { type: "json" };

export default defineConfig({
    lang: "en-US",
    title: "Zep",
    description: "The Zep language reference and standard library documentation.",

    markdown: {
        languages: [{ ...grammar, name: "zep" }],
    },

    themeConfig: {
        nav: [
            { text: "Guide", link: "/getting_started" },
            { text: "Language", link: "/syntax" },
            { text: "Standard library", link: "/stdlib" },
        ],

        sidebar: [
            {
                text: "Start",
                items: [
                    { text: "Overview", link: "/" },
                    { text: "Getting started", link: "/getting_started" },
                    { text: "Projects and commands", link: "/projects" },
                ],
            },
            {
                text: "Language",
                items: [
                    { text: "Syntax", link: "/syntax" },
                    { text: "Types", link: "/types" },
                    { text: "Declarations", link: "/declarations" },
                    { text: "Modules", link: "/modules" },
                    { text: "Expressions", link: "/expressions" },
                    { text: "Generics and interfaces", link: "/generics" },
                    { text: "Ownership and memory", link: "/memory" },
                    { text: "Checked errors", link: "/errors" },
                    { text: "FFI and attributes", link: "/ffi" },
                ],
            },
            {
                text: "Reference",
                items: [
                    { text: "Testing", link: "/testing" },
                    { text: "Standard library", link: "/stdlib" },
                    { text: "Editor support", link: "/editor" },
                    { text: "Compiler architecture", link: "/architecture" },
                ],
            },
        ],

        search: { provider: "local" },
        socialLinks: [{ icon: "github", link: "https://github.com/budchirp/zep-lang" }],
        outline: [2, 3],
    },
});
