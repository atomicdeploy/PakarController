// Each child must succeed; PowerShell's last-command exit status is insufficient.
import {spawnSync} from 'node:child_process';
import {resolve} from 'node:path';
import {fileURLToPath} from 'node:url';
export const phases={
  "quality": [
    ["node", "tests/windows_tray_test.mjs"],
    ["node", "tests/tasks_wiring_test.mjs"],
    ["node", "tests/s3_pin_mapping_test.mjs"],
    ["node", "tests/web_streaming_test.mjs"],
    ["pwsh", "-File", "tools/consult-reuse.ps1", "-Concern", "web-streaming", "-Gate"],
    ["node", "tests/web_peer_http_test.mjs"],
    ["node", "tests/peer_fence_test.mjs"],
    ["pwsh", "-File", "tools/consult-reuse.ps1", "-Concern", "peer-binding", "-Gate"],
    ["node", "tests/native_dropdown_test.mjs"],
    ["node", "tests/github_conversation_audit_test.mjs"],
    ["pwsh", "-File", "tools/consult-reuse.ps1", "-Concern", "terminal-ui", "-Gate"],
    ["pwsh", "-File", "tools/consult-reuse.ps1", "-Concern", "api-contract", "-Gate"],
    ["node", "tools/check-living-contract.mjs"],
    ["node", "tests/living_contract_test.mjs"],
    ["pwsh", "-File", "tools/consult-reuse.ps1", "-Concern", "web-presentation", "-Gate"],
    ["node", "tools/generate-font-assets.mjs", "--check"],
    ["node", "tests/font_gzip_test.mjs"],
    ["node", "tests/browser_console_test.mjs"],
    ["node", "tests/toasts_test.mjs"],
    ["node", "tests/realtime_retry_test.mjs"],
    ["node", "tests/build_script_security_test.mjs"],
    ["node", "tests/native_render_lifecycle_test.mjs"],
    ["node", "tests/localization_test.mjs"],
    ["node", "tests/rayanlamp_reuse_test.mjs"],
    ["node", "tests/rayanlamp_theme_test.mjs"],
    ["node", "tests/rayanlamp_locale_test.mjs"],
    [
      "node",
      "tests/web_display_test.mjs"
    ],
    [
      "node",
      "tools/generate-web-appearance.mjs",
      "--check"
    ],
    [
      "node",
      "tests/web_appearance_test.mjs"
    ],
    [
      "node",
      "tests/web_cell_table_test.mjs"
    ],
    [
      "node",
      "tests/web_context_test.mjs"
    ],
    [
      "node",
      "tests/web_identity_test.mjs"
    ],
    [
      "node",
      "tests/screenshot_harness_test.mjs"
    ],
    [
      "node",
      "tests/native_menu_test.mjs"
    ],
    [
      "node",
      "tests/connection_controls_test.mjs"
    ],
    [
      "node",
      "tools/generate-interface-contract.mjs",
      "--check"
    ],
    [
      "node",
      "tests/interface_settings_test.mjs"
    ],
    [
      "node",
      "tools/generate-branding.mjs",
      "--check"
    ],
    [
      "node",
      "tests/branding_test.mjs"
    ],
    [
      "node",
      "tests/web_access_test.mjs"
    ],
    [
      "node",
      "tests/web_remote_test.mjs"
    ],
    [
      "node",
      "tests/remote_tools_test.mjs"
    ],
    [
      "node",
      "tests/pu_routes_test.mjs"
    ],
    [
      "node",
      "tests/boot_audio_test.mjs"
    ],
    [
      "pwsh",
      "-File",
      "tools/check-reuse-ledger.ps1"
    ],
    [
      "pwsh",
      "-File",
      "tests/reuse_ledger_test.ps1"
    ],
    [
      "pwsh",
      "-File",
      "tools/generate-file-comparison.ps1",
      "-Check"
    ],
    [
      "pwsh",
      "-File",
      "tests/file_comparison_test.ps1"
    ],
    [
      "pwsh",
      "-File",
      "tools/verify-versions.ps1"
    ],
    [
      "pwsh",
      "-File",
      "tests/reuse_preflight_test.ps1"
    ],
    [
      "pwsh",
      "-File",
      "tests/build_storage_test.ps1"
    ],
    [
      "pwsh",
      "-File",
      "tests/storage_hygiene_test.ps1"
    ],
    [
      "node",
      "tools/check-crate-boundary.mjs"
    ],
    [
      "node",
      "tests/crate_boundary_test.mjs"
    ],
    [
      "node",
      "tools/generate-readme-artwork.mjs",
      "--check"
    ],
    [
      "node",
      "tests/repository_governance_test.mjs"
    ]
  ],
  "desktop": [
    [
      "pwsh",
      "-File",
      "tools/build-desktop.ps1",
      "-Mode",
      "Test"
    ],
    [
      "pwsh",
      "-File",
      "tools/build-desktop.ps1",
      "-Mode",
      "Release"
    ],
    [
      "node",
      "tests/terminal_ui_test.mjs"
    ],
    [
      "node",
      "tests/device_cli_test.mjs"
    ],
    [
      "node",
      "tests/pc_contract_cli_test.mjs"
    ],
    [
      "node",
      "tests/remote_cli_test.mjs"
    ],
    [
      "node",
      "tests/pc_contract_mapping_test.mjs"
    ]
  ]
};
export function runChecks(commands,execute=spawnSync){
for(const [cmd,...args] of commands){
  console.log('\n> '+cmd+' '+args.join(' '));
  const r=execute(cmd,args,{stdio:'inherit',shell:false});
  if(r.error)throw r.error;
  if(r.status!==0){const error=Error('Required check failed: '+cmd+' '+args.join(' '));error.exitCode=r.status??1;throw error;}
}
}
if(process.argv[1]&&resolve(process.argv[1])===fileURLToPath(import.meta.url)){
  const phase=process.argv[2];
  if(!Object.hasOwn(phases,phase))throw Error('Usage: node tools/ci-checks.mjs quality|desktop');
  try{runChecks(phases[phase]);}catch(error){console.error(error);process.exit(error.exitCode??1);}
}
