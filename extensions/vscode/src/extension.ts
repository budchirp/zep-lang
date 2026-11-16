
import * as vscode from "vscode";
import { LanguageClient } from "vscode-languageclient/node";

let client: LanguageClient;

export async function activate() {
    const config = vscode.workspace.getConfiguration("zep.server");

    client = new LanguageClient("zep", "Zep", {
        command: config.get<string>("path", "zep"),
        args: config.get<string[]>("args", ["lsp"]),
    }, {
        documentSelector: [{ language: "zep", scheme: "file" }],
    });

    await client.start();
}

export async function deactivate() {
    await client?.dispose();
}
