using CommunityToolkit.Mvvm.ComponentModel;
using Somatic.Model;

namespace Somatic.ViewModels {
    public partial class PageViewModel : ViewModelBase {
        [ObservableProperty] private ApplicationPageNames _pageName;
        [ObservableProperty] private string _title = null!;
    }
}
