# Yetenek Sistemi Kurulum Rehberi

## 1. Unreal Editor'da Yapılacaklar

### Adım 1: Build Et
1. Unreal Editor açık değilse: `MOBAMMO.uproject` dosyasına çift tıkla
2. Sol üstte **Compile** butonuna bas veya `Ctrl+Shift+B`
3. Build tamamlanana kadar bekle (5-10 dakika sürebilir)

### Adım 2: BP_ThirdPersonCharacter Ayarları
1. Content Browser'da: `Content/ThirdPerson/Blueprints/BP_ThirdPersonCharacter`
2. Blueprint'i aç
3. **Class Settings** panelinde:
   - **Parent Class**: `MOBAMMOCharacter` seç (dropdown'da görünmüyorsa Build tamamlanamamış demektir)
4. **Compile** → **Save**

### Adım 3: Input Actions Oluştur
1. Content Browser'da: `Content/Input/Actions` klasörüne git
2. Sağ tık → **Input** → **Input Action**
3. Bunları oluştur:
   - `IA_Ability1` - Tuş: `1`
   - `IA_Ability2` - Tuş: `2`
   - `IA_Ability3` - Tuş: `3`
   - `IA_Ability4` - Tuş: `4`
   - `IA_Ability5` - Tuş: `5`
   - `IA_Ability6` - Tuş: `6`

### Adım 4: IMC_Default Güncelle
1. Content Browser'da: `Content/Input/IMC_Default`
2. Aç ve **Mappings** bölümüne git
3. Yeni mappings ekle:
   - Action: `IA_Ability1` → `1`
   - Action: `IA_Ability2` → `2`
   - Action: `IA_Ability3` → `3`
   - Action: `IA_Ability4` → `4`
   - Action: `IA_Ability5` → `5`
   - Action: `IA_Ability6` → `6`
4. **Save**

### Adım 5: Blueprint'te Input Binding
1. `BP_ThirdPersonCharacter` blueprint'ini aç
2. **Event Graph**'te sağ tık → `IA_Ability1` arat ve ekle
3. Şu node'ları ekle:
   ```
   IA_Ability1 (Pressed) → Get Ability Component → Try Activate Ability (Slot: 0)
   IA_Ability2 (Pressed) → Get Ability Component → Try Activate Ability (Slot: 1)
   ... (diğer yetenekler için tekrarla)
   ```
4. **Compile** → **Save**

### Adım 6: Ability Data Asset Oluştur
1. Content Browser'da yeni klasör: `Content/Abilities`
2. Sağ tık → **Blueprints** → **Data Asset**
3. **Select Asset class**: `AbilityDataAsset`
4. `DA_FighterAbilities` olarak kaydet
5. Aç ve 5 yetenek ekle:
   - Slot 0: `InstantAbility` - "Arc Burst" (10 mana, 2s CD)
   - Slot 1: `InstantAbility` - "Renew" (8 mana, 4s CD)
   - Slot 2: `ProjectileAbility` - "Drain Pulse" (12 mana, 3s CD)
   - Slot 3: `InstantAbility` - "Mana Surge" (0 mana, 6s CD)
   - Slot 4: `InstantAbility` - "Reforge" (0 mana, 0s CD)

### Adım 7: Character'a Data Asset Bağla
1. `BP_ThirdPersonCharacter` blueprint'ini aç
2. Details panelinde **Abilities** kategorisini bul
3. **Ability Data Asset** slotuna `DA_FighterAbilities` sürükle
4. **Compile** → **Save**

## 2. Test Etme
1. **Play** → **Standalone Game**
2. Login ekranında "Mock Login" yap
3. Karakter seç ve server'a bağlan
4. Oyun içinde **1-6** tuşlarına basarak yetenekleri test et
