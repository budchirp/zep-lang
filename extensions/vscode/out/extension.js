"use strict";
var __createBinding = (this && this.__createBinding) || (Object.create ? (function(o, m, k, k2) {
    if (k2 === undefined) k2 = k;
    var desc = Object.getOwnPropertyDescriptor(m, k);
    if (!desc || ("get" in desc ? !m.__esModule : desc.writable || desc.configurable)) {
      desc = { enumerable: true, get: function() { return m[k]; } };
    }
    Object.defineProperty(o, k2, desc);
}) : (function(o, m, k, k2) {
    if (k2 === undefined) k2 = k;
    o[k2] = m[k];
}));
var __setModuleDefault = (this && this.__setModuleDefault) || (Object.create ? (function(o, v) {
    Object.defineProperty(o, "default", { enumerable: true, value: v });
}) : function(o, v) {
    o["default"] = v;
});
var __importStar = (this && this.__importStar) || (function () {
    var ownKeys = function(o) {
        ownKeys = Object.getOwnPropertyNames || function (o) {
            var ar = [];
            for (var k in o) if (Object.prototype.hasOwnProperty.call(o, k)) ar[ar.length] = k;
            return ar;
        };
        return ownKeys(o);
    };
    return function (mod) {
        if (mod && mod.__esModule) return mod;
        var result = {};
        if (mod != null) for (var k = ownKeys(mod), i = 0; i < k.length; i++) if (k[i] !== "default") __createBinding(result, mod, k[i]);
        __setModuleDefault(result, mod);
        return result;
    };
})();
Object.defineProperty(exports, "__esModule", { value: true });
exports.activate = activate;
exports.deactivate = deactivate;
const vscode = __importStar(require("vscode"));
const node_1 = require("vscode-languageclient/node");
let client;
let pending = Promise.resolve();
function restart() {
    pending = pending.catch(() => { }).then(async () => {
        await client?.dispose();
        const config = vscode.workspace.getConfiguration("zep.server");
        client = new node_1.LanguageClient("zep", "Zep", {
            command: config.get("path", "zep"),
            args: config.get("arguments", ["lsp"]),
        }, {
            documentSelector: [{ language: "zep", scheme: "file" }],
        });
        await client.start();
    });
    return pending;
}
async function activate(context) {
    context.subscriptions.push(vscode.commands.registerCommand("zep.restartServer", restart), vscode.workspace.onDidChangeConfiguration(event => {
        if (event.affectsConfiguration("zep.server")) {
            void restart().catch(error => {
                void vscode.window.showErrorMessage(`Zep language server restart failed: ${error}`);
            });
        }
    }));
    await restart();
}
async function deactivate() {
    await pending.catch(() => { });
    await client?.dispose();
}
//# sourceMappingURL=extension.js.map