function Get-RayanLampFiles {
    param([string]$Root)
    $Root = [IO.Path]::GetFullPath($Root).TrimEnd('\','/')
    # Prune generated trees before traversal; retain vendor source and licenses.
    $pending = [Collections.Generic.Stack[string]]::new()
    $pending.Push($Root)
    $paths = [Collections.Generic.List[string]]::new()
    while ($pending.Count) {
        foreach ($item in (Get-ChildItem -LiteralPath $pending.Pop() -Force)) {
            $relative = $item.FullName.Substring($Root.Length).TrimStart('\','/').Replace('\','/')
            if ($item.Attributes -band [IO.FileAttributes]::ReparsePoint) { throw "Donor symlink requires explicit inventory: $relative" }
            if ($item.PSIsContainer) {
                if ($item.Name -in @('.git','node_modules','.cache','dist','dist-sdk','work','coverage')) { continue }
                $pending.Push($item.FullName)
            } else { $paths.Add($relative) }
        }
    }
    return $paths | Sort-Object
}
