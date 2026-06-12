# TWE
<img width="1254" height="1254" alt="twe-logo" src="https://github.com/user-attachments/assets/ee739c68-5eae-4705-9f8c-86c7ead0ed83" />
<img width="1179" height="760" alt="image" src="https://github.com/user-attachments/assets/db5da4e4-ff91-4a4e-909a-feb825771053" />
<img width="1920" height="1049" alt="image" src="https://github.com/user-attachments/assets/329dcb96-69b0-4c0e-851b-245c2694c640" />




[![C++](https://img.shields.io/badge/C%2B%2B-17-00599C?style=for-the-badge&logo=cplusplus&logoColor=white)](https://isocpp.org/)
[![Qt](https://img.shields.io/badge/Qt-5.15.2-41CD52?style=for-the-badge&logo=qt&logoColor=white)](https://www.qt.io/)
[![Windows](https://img.shields.io/badge/Windows-10%2B-0078D6?style=for-the-badge&logo=windows&logoColor=white)](https://www.microsoft.com/windows)
[![Platform](https://img.shields.io/badge/Platform-x64-173D36?style=for-the-badge)](#requirements)
[![Latest Release](https://img.shields.io/github/v/release/cluckegy/TWE?style=for-the-badge&logo=github&color=277F6C)](https://github.com/cluckegy/TWE/releases/latest)
[![Downloads](https://img.shields.io/github/downloads/cluckegy/TWE/total?style=for-the-badge&logo=github&color=0EA5A8)](https://github.com/cluckegy/TWE/releases)
[![License](https://img.shields.io/github/license/cluckegy/TWE?style=for-the-badge&color=64748B)](https://github.com/cluckegy/TWE/blob/main/LICENSE)
[![Discord](https://img.shields.io/badge/Discord-Join%20Community-5865F2?style=for-the-badge&logo=discord&logoColor=white)](https://discord.gg/YrtTNQwFrH)

TWE is a lightweight Windows desktop tool for monitoring WE internet quota
usage and managing devices connected to your local network.

Developed by **Mohamed Wael (MoGlitch)** and sponsored by **CodeLuck**.

> Thank you for downloading TWE. Support for more networks and additional
> features is coming soon.

## Tech Stack

| Technology | Usage |
| --- | --- |
| C++17 | Core application and native launcher |
| Qt 5.15.2 | Desktop interface, networking, and application services |
| Win32 API | Lightweight installer and first-run downloader |
| Npcap | Network packet handling and device control |
| OpenSSL | Secure HTTPS communication |
| Visual Studio | Windows x64 build toolchain |



**TWE** برنامج ويندوز خفيف لمتابعة استهلاك باقة WE ومعرفة الأجهزة المتصلة بالشبكة، مع أدوات للتحكم في السرعة أو حظر الأجهزة على شبكتك المحلية.

تم تطوير البرنامج بواسطة **Mohamed Wael (MoGlitch)** وبرعاية **CodeLuck**.

## التحميل

1. افتح صفحة [آخر إصدار](https://github.com/cluckegy/TWE/releases/latest).
2. حمّل ملف `TWE-portable.zip`.
3. فك الضغط في أي فولدر مناسب.
4. شغّل `TWE.exe`.

`TWE-portable.zip` هو الملف الموصى به للمستخدمين، لأنه يحتوي البرنامج وكل ملفات التشغيل المطلوبة.

يوجد أيضًا ملف `TWE.exe` منفصل في صفحة الإصدار. هذا مجرد لانشر صغير يقوم بتحميل ملفات التشغيل أول مرة، لكن النسخة المحمولة `TWE-portable.zip` أفضل وأسهل لمعظم المستخدمين.

## المميزات

- عرض المتبقي والمستخدم من باقة WE.
- عرض ميعاد التجديد وانتهاء الباقة.
- تسجيل الدخول إلى My WE مع دعم Captcha.
- حفظ بيانات الدخول محليًا بشكل آمن باستخدام Windows DPAPI.
- فحص الأجهزة المتصلة بالشبكة المحلية.
- البحث والتصفية داخل قائمة الأجهزة.
- تحديد سرعة التحميل والرفع لأجهزة معينة.
- حظر أو إعادة تشغيل الإنترنت لأجهزة معينة.
- تحديث تلقائي لبيانات الباقة.
- يعمل في الخلفية من خلال System Tray.

## المتطلبات

- Windows 10 أو أحدث.
- جهاز 64-bit.
- اتصال إنترنت لتسجيل الدخول وجلب بيانات الباقة.
- حساب My WE صحيح.

ميزات فحص الأجهزة الأساسية تعمل مباشرة. ميزات تحديد السرعة والحظر تحتاج تثبيت [Npcap](https://npcap.com/#download)، وقد تحتاج تشغيل البرنامج كمسؤول Administrator.

## ملاحظات مهمة

- استخدم أدوات التحكم في الشبكة فقط على شبكة تملكها أو لديك إذن بإدارتها.
- بعض برامج الحماية قد تظهر تحذيرًا لأن البرنامج يستخدم وظائف متقدمة للشبكات مثل فحص الأجهزة والتحكم في الاتصال.
- إذا ظهر تحذير من Windows Security، تأكد أنك حملت البرنامج من صفحة GitHub الرسمية فقط.
- إذا واجهت مشكلة في اللانشر، استخدم `TWE-portable.zip` بدلًا منه.

## حل المشاكل الشائعة

### البرنامج لا يفتح أو ملف TWE.exe اختفى

غالبًا Windows Security حذف الملف أو وضعه في الحجر. حمّل `TWE-portable.zip` من صفحة الإصدار الرسمية، ثم فك الضغط في فولدر جديد وشغّل البرنامج.

### تظهر رسالة Captcha ولا تظهر صورة

حمّل آخر إصدار من صفحة Releases. الإصدارات القديمة قد لا تحتوي أحدث إصلاحات تسجيل الدخول.

### لا تظهر الأجهزة أو لا تعمل أدوات التحكم

ثبّت [Npcap](https://npcap.com/#download)، ثم شغّل TWE كمسؤول Administrator. تأكد أيضًا أنك اخترت كارت الشبكة الصحيح من داخل البرنامج.

### روابط GitHub أو Discord لا تفتح

أغلق المتصفح وافتحه بشكل طبيعي، أو شغّل آخر إصدار من TWE. الإصدارات الجديدة تفتح الروابط بطريقة أفضل عند تشغيل البرنامج كمسؤول.

## الخصوصية

- رقم الأرضي يتم حفظه محليًا على جهازك.
- كلمة المرور والجلسة يتم حمايتهم باستخدام Windows DPAPI.
- بيانات الدخول المحمية لا يمكن فكها إلا من نفس مستخدم ويندوز.
- البرنامج يتصل بخدمات WE لتسجيل الدخول وجلب بيانات الباقة فقط.

## روابط

- [آخر إصدار](https://github.com/cluckegy/TWE/releases/latest)
- [كل الإصدارات](https://github.com/cluckegy/TWE/releases)
- [Discord](https://discord.gg/YrtTNQwFrH)
- [GitHub](https://github.com/cluckegy/TWE)

## للمطورين

المشروع مبني باستخدام C++17 و Qt 5.15.2 على Windows x64.

لإنشاء ملفات التوزيع:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\package-release.ps1
```

الملفات النهائية تظهر في:

```text
Export/TWE-Final/
|-- TWE.exe
|-- TWE-portable.zip
|-- TWE-runtime.zip
`-- UPLOAD-INSTRUCTIONS.txt
```

عند نشر إصدار جديد على GitHub، ارفع `TWE-portable.zip` للمستخدمين، وارفع `TWE-runtime.zip` باسم ثابت إذا كنت تريد دعم اللانشر.
