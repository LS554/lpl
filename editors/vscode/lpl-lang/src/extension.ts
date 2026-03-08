// Copyright 2026 London Sheard
// Licensed under the Apache License, Version 2.0.

import * as vscode from 'vscode';
import * as path from 'path';
import {
    LanguageClient,
    LanguageClientOptions,
    ServerOptions,
    TransportKind,
} from 'vscode-languageclient/node';

let client: LanguageClient | undefined;

export function activate(context: vscode.ExtensionContext): void {
    // Resolve server module - the LSP server is shipped inside the extension
    const serverModule = context.asAbsolutePath(path.join('lsp', 'out', 'server.js'));

    const serverOptions: ServerOptions = {
        run: { module: serverModule, transport: TransportKind.ipc },
        debug: { module: serverModule, transport: TransportKind.ipc },
    };

    const clientOptions: LanguageClientOptions = {
        documentSelector: [
            { scheme: 'file', language: 'lpl' },
            { scheme: 'untitled', language: 'lpl' },
        ],
    };

    client = new LanguageClient('lpl', 'LPL Language Server', serverOptions, clientOptions);
    client.start();
}

export function deactivate(): Thenable<void> | undefined {
    if (!client) return undefined;
    return client.stop();
}
