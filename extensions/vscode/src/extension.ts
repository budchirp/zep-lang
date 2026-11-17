
import * as vscode from "vscode";
import { LanguageClient } from "vscode-languageclient/node";

let client: LanguageClient | undefined;
let pending = Promise.resolve();

function restart(): Promise<void> {
    pending = pending.catch(() => {}).then(async () => {
        await client?.dispose();

        const config = vscode.workspace.getConfiguration("zep.server");

        client = new LanguageClient("zep", "Zep", {
            command: config.get<string>("path", "zep"),
            args: config.get<string[]>("arguments", ["lsp"]),
        }, {
            documentSelector: [{ language: "zep", scheme: "file" }],
        });

        await client.start();
    });

    return pending;
}

export async function activate(context: vscode.ExtensionContext): Promise<void> {
    context.subscriptions.push(
        vscode.commands.registerCommand("zep.restartServer", restart),
        vscode.workspace.onDidChangeConfiguration(event => {
            if (event.affectsConfiguration("zep.server")) {
                void restart().catch(error => {
                    void vscode.window.showErrorMessage(`Zep language server restart failed: ${error}`);
                });
            }
        }),
    );

    await restart();
}

export async function deactivate(): Promise<void> {
    await pending.catch(() => {});
    await client?.dispose();
}
