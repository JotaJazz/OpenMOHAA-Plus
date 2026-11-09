// OpenMOHAA Plus: completion and hover docs for MOHAA .scr scripts.
// data/commands.json is generated from the engine by misc/scr-tools/gen_editor_data.py.
const vscode = require('vscode');
const path = require('path');
const fs = require('fs');

function loadCommands() {
    const file = path.join(__dirname, 'data', 'commands.json');
    const byName = new Map();
    for (const cmd of JSON.parse(fs.readFileSync(file, 'utf8')).commands) {
        if (!byName.has(cmd.name)) {
            byName.set(cmd.name, []);
        }
        byName.get(cmd.name).push(cmd);
    }
    return byName;
}

function describe(variants) {
    const md = new vscode.MarkdownString();
    for (const v of variants) {
        md.appendCodeblock(v.signature, 'mohaa-scr');
        md.appendMarkdown(`${v.doc}  \n*${v.classes.join(', ')}*\n\n`);
    }
    return md;
}

function activate(context) {
    const commands = loadCommands();
    const selector = { language: 'mohaa-scr' };

    context.subscriptions.push(vscode.languages.registerCompletionItemProvider(selector, {
        provideCompletionItems() {
            return [...commands].map(([name, variants]) => {
                const item = new vscode.CompletionItem(name, vscode.CompletionItemKind.Function);
                item.detail = variants[0].signature;
                item.documentation = describe(variants);
                return item;
            });
        }
    }));

    context.subscriptions.push(vscode.languages.registerHoverProvider(selector, {
        provideHover(document, position) {
            const range = document.getWordRangeAtPosition(position, /[A-Za-z_][A-Za-z0-9_]*/);
            const variants = range && commands.get(document.getText(range).toLowerCase());
            return variants ? new vscode.Hover(describe(variants), range) : undefined;
        }
    }));
}

function deactivate() {}

module.exports = { activate, deactivate };
