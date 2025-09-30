# VSCode Task Sistemi - Kullanım Rehberi

## 📋 Otomatik Cache Temizleme Task'ları

### 🏭 Ana Build Task'ları (Otomatik Cache Temizleme ile)

**Build Gateway:**
- `Ctrl+Shift+P` → `Tasks: Run Task` → `Build Gateway`
- Otomatik olarak:
  1. 🏭 Gateway moduna geçer
  2. 🧹 CMake cache'i temizler
  3. ⚙️ CMake'i yeniden configure eder
  4. 🔨 Gateway firmware'ini build eder

**Build Node:**
- `Ctrl+Shift+P` → `Tasks: Run Task` → `Build Node`
- Otomatik olarak:
  1. 📡 Node moduna geçer
  2. 🧹 CMake cache'i temizler
  3. ⚙️ CMake'i yeniden configure eder
  4. 🔨 Node firmware'ini build eder

### ⚡ Hızlı Build Task'ları (Cache Temizlemeden)

**Quick Build Gateway:**
- `Ctrl+Shift+P` → `Tasks: Run Task` → `⚡ Quick Build Gateway`
- Sadece build yapar, mode değiştirmez, cache temizlemez

**Quick Build Node:**
- `Ctrl+Shift+P` → `Tasks: Run Task` → `⚡ Quick Build Node`
- Sadece build yapar, mode değiştirmez, cache temizlemez

### 🔄 Tam Clean & Build Task'ları

**Full Clean & Build Gateway:**
- `Ctrl+Shift+P` → `Tasks: Run Task` → `🔄 Full Clean & Build Gateway`
- Kapsamlı build süreci

**Full Clean & Build Node:**
- `Ctrl+Shift+P` → `Tasks: Run Task` → `🔄 Full Clean & Build Node`
- Kapsamlı build süreci

### 🛠️ Yardımcı Task'lar

**Show Current Build Mode:**
- `Ctrl+Shift+P` → `Tasks: Run Task` → `📊 Show Current Build Mode`
- Mevcut configuration ve build artifacts'ı gösterir

**Clean CMake Cache:**
- `Ctrl+Shift+P` → `Tasks: Run Task` → `🧹 Clean CMake Cache`
- Sadece cache'i temizler

**Configure CMake:**
- `Ctrl+Shift+P` → `Tasks: Run Task` → `⚙️ Configure CMake`
- Sadece CMake configure eder

**Switch to Gateway Mode:**
- `Ctrl+Shift+P` → `Tasks: Run Task` → `🏭 Switch to Gateway Mode`
- Sadece mode değiştirir ve config headers generate eder

**Switch to Node Mode:**
- `Ctrl+Shift+P` → `Tasks: Run Task` → `📡 Switch to Node Mode`
- Sadece mode değiştirir ve config headers generate eder

### 🔥 Flash Task'ları

**Flash Gateway:**
- `Ctrl+Shift+P` → `Tasks: Run Task` → `STM32: Flash Gateway`
- Gateway firmware'ini STM32'ye yükler

**Flash Node:**
- `Ctrl+Shift+P` → `Tasks: Run Task` → `STM32: Flash Node`
- Node firmware'ini STM32'ye yükler

## 🎯 En Sık Kullanılan Senaryolar

### Senaryo 1: Gateway'den Node'a geçiş
```
1. Ctrl+Shift+P → Tasks: Run Task → Build Node
2. (Otomatik: mode change + cache clean + configure + build)
3. Ctrl+Shift+P → Tasks: Run Task → STM32: Flash Node
```

### Senaryo 2: Hızlı kod değişikliği test
```
1. Kodu düzenle
2. Ctrl+Shift+P → Tasks: Run Task → ⚡ Quick Build Gateway (veya Node)
3. Flash if needed
```

### Senaryo 3: Build sorunları giderme
```
1. Ctrl+Shift+P → Tasks: Run Task → 📊 Show Current Build Mode
2. Ctrl+Shift+P → Tasks: Run Task → 🔄 Full Clean & Build Gateway
```

### Senaryo 4: Default build (F5 or Ctrl+Shift+P → Tasks: Run Build Task)
- `Build Gateway` task'ı default olarak çalışır

## 🔧 Task Dependency Sırası

### Build Gateway:
```
🏭 Switch to Gateway Mode
↓
🧹 Clean CMake Cache  
↓
⚙️ Configure CMake
↓
🔨 Build Gateway
```

### Build Node:
```
📡 Switch to Node Mode
↓
🧹 Clean CMake Cache
↓  
⚙️ Configure CMake
↓
🔨 Build Node
```

## 💡 Pro Tips

1. **Keyboard Shortcuts:** Default build için `Ctrl+Shift+B` kullanın
2. **Mode Check:** Hangi modda olduğunuzdan emin değilseniz `📊 Show Current Build Mode` çalıştırın
3. **Cache Issues:** Build sorunları varsa önce `🧹 Clean CMake Cache` deneyin
4. **Quick Builds:** Küçük değişiklikler için `⚡ Quick Build` task'larını kullanın
5. **Auto Flash:** Build + Flash'ı otomatikleştirmek için Flash task'larının `dependsOn` özelliğini kullanın

## 🚨 Önemli Notlar

- Mode değişikliği her zaman cache temizleme gerektirir
- Quick build task'ları mevcut mode'u korur
- Flash task'ları otomatik olarak build'ı tetikler
- Tüm task'lar terminal çıktısını paylaşır